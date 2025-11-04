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
// Created by Wangyunlai on 2024/05/29.
//

#pragma once

#include "sql/expr/expression.h"
#include "storage/table/table.h"
#include <string>

class BinderContext
{
public:
  BinderContext()          = default;
  virtual ~BinderContext() = default;

  void add_db(Db *db) { db_ = db; }
  void add_table(Table *table) { query_tables_.push_back(table); }
  void set_table_map(unordered_map<string, Table *> *table_map) { table_map_ = table_map; }

  Table *find_table(const char *table_name) const;
  const std::vector<std::string> &alias() { return tables_alias_; }
  bool has_tables_alias() { return !tables_alias_.empty(); }

  const vector<Table *> &query_tables() const { return query_tables_; }

  [[nodiscard]] Table *default_table() const { return default_table_; }
  void                     set_default_table(Table *default_table) { default_table_ = default_table; }
  void set_alias(std::vector<std::string> alias) { tables_alias_ = std::move(alias); }

private:
  Db *db_;
  Table* default_table_;
  vector<Table *>                 query_tables_;
  std::vector<std::string>                      tables_alias_;
  unordered_map<string, Table *> *table_map_;
};

/**
 * @brief 绑定表达式
 * @details 绑定表达式，就是在SQL解析后，得到文本描述的表达式，将表达式解析为具体的数据库对象
 * 主要是匹配字段名到具体的表和字段，还有通配符*展开检测aggerate函数等
 * 其余的都只是递归建立新的表达式节点
 */
class ExpressionBinder
{
public:
  ExpressionBinder(BinderContext &context) : context_(context) { multi_tables_ = context.query_tables().size() > 1; }
  virtual ~ExpressionBinder() = default;

  RC bind_expression(unique_ptr<Expression> &expr, vector<unique_ptr<Expression>> &bound_expressions);

private:
  RC bind_star_expression(unique_ptr<Expression> &star_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_unbound_field_expression(
      unique_ptr<Expression> &unbound_field_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_field_expression(unique_ptr<Expression> &field_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_value_expression(unique_ptr<Expression> &value_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_cast_expression(unique_ptr<Expression> &cast_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_comparison_expression(
      unique_ptr<Expression> &comparison_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_conjunction_expression(
      unique_ptr<Expression> &conjunction_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_arithmetic_expression(
      unique_ptr<Expression> &arithmetic_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_function_expression(unique_ptr<Expression> &function_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_subquery_expression(unique_ptr<Expression> &subquery_expr, vector<unique_ptr<Expression>> &bound_expressions);
  RC bind_list_expression(unique_ptr<Expression> &values_expr, vector<unique_ptr<Expression>> &bound_expressions);

private:
  bool           multi_tables_;
  BinderContext &context_;
};
