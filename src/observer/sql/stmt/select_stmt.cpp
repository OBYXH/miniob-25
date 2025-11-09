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
// Created by Wangyunlai on 2022/6/6.
//

#include "sql/stmt/select_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "sql/expr/expression.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "sql/parser/expression_binder.h"
#include <cstddef>
#include <memory>

using namespace std;
using namespace common;

// 专门为条件表达式递归绑定别名而设的函数，用来把别名从转移到Experssion上
// 不太优雅，但暂时没更好办法
void bind_table_alias_to_condition_expr(const vector<string> &table_aliases, unique_ptr<Expression> &expr)
{
  LOG_DEBUG("try to bind expr,name = %s, type =%s, alias = %s ", expr->name(), expr_type_to_string(expr->type()),expr->field_alias());
  ExprType type = expr->type();
  switch (type) {
    case ExprType::CONJUNCTION: {
      ConjunctionExpr *conj_expr = static_cast<ConjunctionExpr *>(expr.get());
      for (auto &child_expr : conj_expr->children()) {
        bind_table_alias_to_condition_expr(table_aliases, child_expr);
      }
    }
    break;
    case ExprType::COMPARISON: {
      ComparisonExpr *cmp_expr = static_cast<ComparisonExpr *>(expr.get());
      bind_table_alias_to_condition_expr(table_aliases, cmp_expr->left());
      bind_table_alias_to_condition_expr(table_aliases, cmp_expr->right());
    }
    case ExprType::UNBOUND_FIELD: {
      UnboundFieldExpr *ub_field_expr = static_cast<UnboundFieldExpr *>(expr.get());
      // 如果 unbound_field_expr 的 table_alias 在 table_aliases 中, 说明是别名, 绑定到Expression上
      if (find(table_aliases.begin(), table_aliases.end(), ub_field_expr->table_name()) != table_aliases.end()) {
        ub_field_expr->set_table_alias(ub_field_expr->table_name());
      }
    }
    break;
    default: 
    break;
  }
}


SelectStmt::~SelectStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
  if (nullptr != having_filter_stmt_) {
    delete having_filter_stmt_;
    having_filter_stmt_ = nullptr;
  }
}

RC SelectStmt::create(Db *db, SelectSqlNode &select_sql, Stmt *&stmt,
    std::shared_ptr<std::vector<string>> loaded_relation_names, unordered_map<string, Table *> outer_table_map)
{
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }

  if (select_sql.expressions.empty()) {
    LOG_WARN("invalid argument. select expr is empty");
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }
  if (loaded_relation_names == nullptr)
    loaded_relation_names = std::make_shared<std::vector<string>>();

  BinderContext binder_context;

  // collect tables in `from` statement
  vector<Table *>                tables;
  unordered_map<string, Table *> table_map;
  // 继承外面的 table_map
  if (!outer_table_map.empty()) {
    for (auto &table_pair : outer_table_map) {
      LOG_DEBUG("filter stmt: table map: (%s, %s)", table_pair.first.c_str(), table_pair.second->name());
    }
    table_map = outer_table_map;
  }

  // 首先将 loaded_relation_names 中的表名添加到 table_map 中
  // 由于处理子查询是递归进行的，只会由外向内传，所以内层的 sub select
  // 会额外拥有外层扫到的 table，而外层不会。
  for (auto &rel_name : *loaded_relation_names) {
    Table *table = db->find_table(rel_name.c_str());
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), rel_name.c_str());
      return RC_WITH_LOCATION(RC::SCHEMA_TABLE_NOT_EXIST, "");
    }
    table->set_is_outer_table(true);
    table_map.insert({rel_name, table});
  }

  // 然后才是处理 select 语句中的 from 语句
  std::vector<std::string> tables_alias; // 记录所有表的别名，顺序和 tables 保持一致
  map<string, string> table_alias_map; // 记录表别名到表名的映射
  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    const char *table_name = select_sql.relations[i].relation_name.c_str();
    if (nullptr == table_name) {
      LOG_WARN("invalid argument. relation name is null. index=%d", i);
      return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
    }

    Table *table = db->find_table(table_name);
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
      return RC_WITH_LOCATION(RC::SCHEMA_TABLE_NOT_EXIST, "");
    }

    binder_context.add_table(table);
    tables.emplace_back(table);
    loaded_relation_names->push_back(table_name);
    tables_alias.push_back(select_sql.relations[i].ralation_alias);
    table_alias_map[select_sql.relations[i].ralation_alias] = table_name;

    // 检查 alias 重复
    for (size_t j = i + 1; j < select_sql.relations.size(); j++) {
      if (select_sql.relations[i].ralation_alias.empty() || select_sql.relations[j].ralation_alias.empty())
        continue;
      if (select_sql.relations[i].ralation_alias == select_sql.relations[j].ralation_alias) {
        LOG_WARN("duplicate alias: %s", select_sql.relations[i].ralation_alias.c_str());
        return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
      }
    }

    auto table_alias = select_sql.relations[i].ralation_alias;
    if (!table_alias.empty()) {
      table_map[table_alias] = table;
    } else {
      table_map[table_name] = table;
    }
  }

  // 下面做的是绑定表达式操作，各种新算子都需要走下面流程

  // table_map.insert(table_alias_map.begin(), table_alias_map.end());
  binder_context.set_table_map(&table_map);
  // collect query fields in `select` statement
  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder               expression_binder(binder_context);

  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    // 如果是 StarExpr，检查是否有别名，如果有报错
    if (expression->type() == ExprType::STAR) {
      StarExpr *star_expr = static_cast<StarExpr *>(expression.get());
      if (!is_blank(star_expr->field_alias())) {
        LOG_WARN("alias found in star expression");
        return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
      }
    }

    RC rc = expression_binder.bind_expression(expression, bound_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> group_by_expressions;
  for (unique_ptr<Expression> &expression : select_sql.group_by) {
    RC rc = expression_binder.bind_expression(expression, group_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  // 子查询，遍历 conditions 中的表达式，（递归）创建对应的 stmt。
  // 这个 for 会将所有的子查询的 stmt 都创建好，放到 SubqueryExpr 中
  for (auto &condition : select_sql.conditions) {
    // exists/not exists 可能会使得 left_expr 为空
    if (condition.left != nullptr && condition.left->type() == ExprType::SUBQUERY) {
      SubqueryExpr *subquery_expr = static_cast<SubqueryExpr *>(condition.left.get());
      Stmt         *stmt          = nullptr;
      RC rc = SelectStmt::create(db, subquery_expr->sub_query_sn()->selection, stmt, loaded_relation_names, table_map);
      if (rc != RC::SUCCESS) {
        LOG_WARN("cannot construct subquery stmt");
        return rc;
      }
      // 检查子查询的合法性：子查询的查询的属性只能有一个, 但exists除外
      if (condition.comp != EXISTS_OP && condition.comp != NOT_EXISTS_OP) {
        RC rc_ = check_sub_select_legal(db, subquery_expr->sub_query_sn());
        if (rc_ != RC::SUCCESS) {
          return rc_;
        }
      }
      subquery_expr->set_stmt(unique_ptr<SelectStmt>(static_cast<SelectStmt *>(stmt)));
    }
    if (condition.right != nullptr && condition.right->type() == ExprType::SUBQUERY) {
      SubqueryExpr *subquery_expr = static_cast<SubqueryExpr *>(condition.right.get());
      Stmt         *stmt          = nullptr;
      RC rc = SelectStmt::create(db, subquery_expr->sub_query_sn()->selection, stmt, loaded_relation_names, table_map);
      if (rc != RC::SUCCESS) {
        LOG_WARN("cannot construct subquery stmt");
        return rc;
      }
      // 检查子查询的合法性：子查询的查询的属性只能有一个, 但exists除外
      if (condition.comp != EXISTS_OP && condition.comp != NOT_EXISTS_OP) {
        RC rc_ = check_sub_select_legal(db, subquery_expr->sub_query_sn());
        if (rc_ != RC::SUCCESS) {
          return rc_;
        }
      }
      subquery_expr->set_stmt(unique_ptr<SelectStmt>(static_cast<SelectStmt *>(stmt)));
    }

    bind_table_alias_to_condition_expr(tables_alias, condition.left);
    bind_table_alias_to_condition_expr(tables_alias, condition.right);
  }

  vector<unique_ptr<Expression>> order_by_expressions;
  for (OrderBySqlNode &unit : select_sql.order_by) {
    RC rc = expression_binder.bind_expression(unit.expr, order_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  std::vector<OrderBySqlNode> order_by_;
  order_by_.reserve(order_by_expressions.size());
  for (size_t i = 0; i < order_by_expressions.size(); i++) {
    order_by_.push_back({std::move(order_by_expressions[i]), select_sql.order_by[i].is_asc});
  }

  int limit = -1;
  if (select_sql.limit >= 0) {
    // bind limit
    limit = select_sql.limit;
  }

  Table *default_table = nullptr;
  if (tables.size() == 1) {
    default_table = tables[0];
  }

  // create filter statement in `where` statement
  FilterStmt *filter_stmt = nullptr;
  RC          rc =
      FilterStmt::create(db, default_table, &table_map, select_sql.conditions, filter_stmt, FilterStmt::Type::WHERE);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }

  FilterStmt *having_filter_stmt = nullptr;
  rc                             = FilterStmt::create(
      db, default_table, &table_map, select_sql.having_conditions, having_filter_stmt, FilterStmt::Type::HAVING);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct having filter stmt");
    return rc;
  }
  // everything alright
  SelectStmt *select_stmt = new SelectStmt();

  select_stmt->tables_.swap(tables);
  select_stmt->table_alias_.swap(tables_alias);
  select_stmt->query_expressions_.swap(bound_expressions);
  select_stmt->filter_stmt_        = filter_stmt;
  select_stmt->having_filter_stmt_ = having_filter_stmt;
  select_stmt->group_by_.swap(group_by_expressions);
  select_stmt->order_by_.swap(order_by_);
  select_stmt->limit_ = limit;
  stmt                = select_stmt;
  return RC::SUCCESS;
}
