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
// Created by Wangyunlai on 2022/6/5.
//

#pragma once

#include "common/sys/rc.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/stmt.h"
#include "storage/field/field.h"

class FieldMeta;
class FilterStmt;
class Db;
class Table;

/**
 * @brief 表示select语句
 * @ingroup Statement
 */
class SelectStmt : public Stmt
{
public:
  SelectStmt() = default;
  ~SelectStmt() override;

  StmtType type() const override { return StmtType::SELECT; }

public:
  static RC create(Db *db, SelectSqlNode &select_sql, Stmt *&stmt,
      std::shared_ptr<std::vector<string>> loaded_relation_names = nullptr,
      unordered_map<string, Table *>       outer_table_map       = {});

public:
  const vector<Table *> &tables() const { return tables_; }
  FilterStmt            *filter_stmt() const { return filter_stmt_; }
  FilterStmt            *having_filter_stmt() const { return having_filter_stmt_; }

  vector<unique_ptr<Expression>> &query_expressions() { return query_expressions_; }
  vector<unique_ptr<Expression>> &group_by() { return group_by_; }
  std::vector<OrderBySqlNode>    &order_by() { return order_by_; }
  int                             limit() const { return limit_; }
  vector<unique_ptr<Expression>> &having_expressions()
  {
    return having_filter_stmt_ ? having_filter_stmt_->conditions() : *(new vector<unique_ptr<Expression>>());
  }

  std::vector<FieldMeta> get_query_fields() {
    std::vector<FieldMeta> query_fields;

    for (auto &expr : query_expressions_) {
      if (expr->type() == ExprType::FIELD) {
        // 此时已经将所有的 UnboundFieldExpr 转换为 FieldExpr
        auto field_expr = static_cast<FieldExpr *>(expr.get());
        FieldMeta field_meta(*field_expr->field().meta());
        field_meta.table_name_ = (*field_expr).table_name(); // 记录表名
        if (!string(expr->field_alias()).empty()) {
          // 别名覆盖字段名
          field_meta.set_name(expr->field_alias());
        }
        query_fields.push_back(field_meta);
      } else {
        FieldMeta field_meta;
        std::string field_name;
        if (!string(expr->field_alias()).empty()) {
          // 别名覆盖字段名
          field_name = expr->field_alias();
        } else {
          field_name = expr ->name();
        }
        field_meta.init(field_name.c_str(), expr->value_type(), 0, expr->value_length(), true, 0);
        query_fields.push_back(field_meta);
      }
    }

    return query_fields;
  }

  bool has_aggr() {
    for (auto &expr : query_expressions_) {
      if (expr->type() == ExprType::AGGREGATION){
        return true;
      }
    }
    return false;
  }

  bool has_arithmatic() {
    for (auto &expr : query_expressions_) {
      if (expr->type() == ExprType::ARITHMETIC) {
        return true;
      }
    }
    return false;
  }

  bool has_join() {
    return tables_.size() > 1;
  }
  
private:
  vector<unique_ptr<Expression>> query_expressions_;
  vector<Table *>                tables_;
  FilterStmt                    *filter_stmt_        = nullptr;
  FilterStmt                    *having_filter_stmt_ = nullptr;
  vector<unique_ptr<Expression>> group_by_;
  std::vector<OrderBySqlNode>    order_by_;
  int                            limit_ = -1;
  vector<string>       table_alias_;
};
