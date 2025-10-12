/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/filter_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/sys/rc.h"
#include "sql/expr/expression.h"
#include "sql/parser/expression_binder.h"
#include "sql/parser/parse_defs.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include <cstddef>
#include <memory>
#include <vector>

FilterStmt::~FilterStmt() { conditions_.clear(); }

RC FilterStmt::create(Db *db, Table *default_table, unordered_map<string, Table *> *tables,
    std::vector<ConditionSqlNode> &conditions, FilterStmt *&stmt)
{
  RC rc = RC::SUCCESS;
  stmt  = nullptr;

  vector<unique_ptr<Expression>> cond_exprs;
  for (auto &condition : conditions) {
    switch (condition.comp) {
      case EQUAL_TO:
      case LESS_EQUAL:
      case NOT_EQUAL:
      case LESS_THAN:
      case GREAT_EQUAL:
      case GREAT_THAN: 
      case LIKE_OP: 
      case NOT_LIKE_OP:
      case IS_OP:
      case IS_NOT_OP: {
        cond_exprs.emplace_back(
            new ComparisonExpr(condition.comp, std::move(condition.left), std::move(condition.right)));
      } break;
      default: {
        LOG_WARN("unsupported condition comparison type: %d", condition.comp);
        return RC::UNIMPLEMENTED;
      }
    }
  }
  
  // 使用下面的绑定逻辑替代原本极其有限的过滤表达式处理
  BinderContext context;
  for (auto &table : *tables) {
    context.add_table(table.second);
  }

  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder expr_binder(context);

  FilterStmt *final_stmt = new FilterStmt();
  for (size_t i = 0; i < conditions.size(); i++) {
     RC rc = expr_binder.bind_expression(cond_exprs[i],bound_expressions);
     if (rc != RC::SUCCESS) {
        delete final_stmt;
        LOG_WARN("failed to bind expression in condition %d", i);
        return rc;
     }
  }
  
  final_stmt->conditions_.swap(bound_expressions);
  stmt = final_stmt;
  return rc;
}

RC get_table_and_field(Db *db, Table *default_table, unordered_map<string, Table *> *tables, const RelAttrSqlNode &attr,
    Table *&table, const FieldMeta *&field)
{
  if (common::is_blank(attr.relation_name.c_str())) {
    table = default_table;
  } else if (nullptr != tables) {
    auto iter = tables->find(attr.relation_name);
    if (iter != tables->end()) {
      table = iter->second;
    }
  } else {
    table = db->find_table(attr.relation_name.c_str());
  }
  if (nullptr == table) {
    LOG_WARN("No such table: attr.relation_name: %s", attr.relation_name.c_str());
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  field = table->table_meta().field(attr.attribute_name.c_str());
  if (nullptr == field) {
    LOG_WARN("no such field in table: table %s, field %s", table->name(), attr.attribute_name.c_str());
    table = nullptr;
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  return RC::SUCCESS;
}
