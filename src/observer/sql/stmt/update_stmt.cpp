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

#include "sql/stmt/update_stmt.h"
#include "common/lang/unordered_map.h"
#include "common/sys/rc.h"
#include "common/value.h"
#include "sql/expr/expression.h"
#include "sql/parser/expression_binder.h"
#include "sql/stmt/select_stmt.h"
#include "storage/db/db.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/field/field_meta.h"
#include <memory>
#include <utility>
#include <vector>

UpdateStmt::UpdateStmt(
    Table *table, vector<unique_ptr<Expression>> exprs, vector<FieldMeta> field_metas, FilterStmt *filter_stmt)
    : table_(table), exprs_(std::move(exprs)), field_metas_(std::move(field_metas)), filter_stmt_(filter_stmt)
{}

UpdateStmt::~UpdateStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC UpdateStmt::create(Db *db, UpdateSqlNode &update, Stmt *&stmt)
{
  // TODO
  const char *table_name = update.relation_name.c_str();
  if (nullptr == db || nullptr == table_name || update.update_list.size() == 0) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, value_num=%d",
        db, table_name, static_cast<int>(update.update_list.size()));
    return RC::INVALID_ARGUMENT;
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // 某些视图不支持更新
  if (table->is_view()) {
    auto *view = static_cast<View *>(table);
    if (!view->is_updatable()) {
      LOG_WARN("the target table(view) of the INSERT is not updatable");
      return RC::INVALID_ARGUMENT;
    }
  }

  unordered_map<std::string, Table *> table_map;
  table_map.insert(pair<string, Table *>(string(table_name), table));

  FilterStmt *filter_stmt = nullptr;
  RC          rc = FilterStmt::create(db, table, &table_map, update.conditions, filter_stmt, FilterStmt::Type::WHERE);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  TableMeta     meta = table->table_meta();
  BinderContext context;
  context.add_table(table);
  context.set_table_map(&table_map);
  ExpressionBinder                    binder(context);
  std::vector<unique_ptr<Expression>> bound_expressions;
  std::vector<FieldMeta>              field_metas;
  for (const auto &[attr, expr] : update.update_list) {
    auto field_meta = meta.field(attr.c_str());
    if (field_meta == nullptr) {
      LOG_WARN("no such field. table=%s, field=%s", table_name, attr.c_str());
      return RC::SCHEMA_FIELD_NOT_EXIST;
    }

    if (expr->type() == ExprType::SUBQUERY) {
      auto  subquery_expr = static_cast<SubqueryExpr *>(expr);
      Stmt *stmt          = nullptr;
      RC    rc            = SelectStmt::create(db, subquery_expr->sub_query_sn()->selection, stmt);
      if (rc != RC::SUCCESS) {
        LOG_WARN("failed to create sub select statement");
        return rc;
      }
      // 检查子查询是否合法，属性只能有一个
      RC rc_ = Stmt::check_sub_select_legal(db, subquery_expr->sub_query_sn());
      if (rc_ != RC::SUCCESS) {
        return rc_;
      }
      subquery_expr->set_stmt(unique_ptr<SelectStmt>(static_cast<SelectStmt *>(stmt)));
    }
    unique_ptr<Expression> exprp(expr);
    RC                     rc = binder.bind_expression(exprp, bound_expressions);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to bind expression");
      return rc;
    }
    field_metas.push_back(*field_meta);
  }
  stmt = new UpdateStmt(table, std::move(bound_expressions), std::move(field_metas), filter_stmt);
  return rc;
}
