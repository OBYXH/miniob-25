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

#pragma once

#include "common/sys/rc.h"
#include "common/value.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/field/field_meta.h"

class Table;

/**
 * @brief 更新语句
 * @ingroup Statement
 */
class UpdateStmt : public Stmt
{
public:
  UpdateStmt() = default;
  ~UpdateStmt() override;
  UpdateStmt(Table *table, vector<FieldMeta> field_metas, vector<unique_ptr<Expression>> values,
      FilterStmt *filter_stmt);
  StmtType    type() const override { return StmtType::UPDATE; }
  FilterStmt *filter_stmt() const { return filter_stmt_; }

public:
  static RC create(Db *db, UpdateSqlNode &update_sql, Stmt *&stmt);

public:
  Table                               *table() const { return table_; }
  vector<unique_ptr<Expression>>      &values() { return values_; }
  vector<FieldMeta>                    field_metas() const { return field_metas_; }

private:
  Table                              *table_ = nullptr;
  vector<FieldMeta>                   field_metas_;
  vector<unique_ptr<Expression>> values_;
  FilterStmt                         *filter_stmt_ = nullptr;
};
