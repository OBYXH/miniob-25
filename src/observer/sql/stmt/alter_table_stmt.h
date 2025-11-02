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
#include "common/type/attr_type.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/stmt.h"
#include "storage/field/field.h"

class FieldMeta;
class Db;
class Table;

/**
 * @brief 表示alter语句
 * @ingroup Statement
 */
class AlterTableStmt : public Stmt
{
public:
  AlterTableStmt() = default;
  AlterTableStmt(Table *table, AlterType alter_type, string new_attribute_name, AttrInfoSqlNode attr_info,
      string new_table_name, string index_name = "", vector<FieldMeta> field_meta = {})
      : table_(table),
        alter_type_(alter_type),
        attr_info_(attr_info),
        new_attribute_name_(new_attribute_name),
        new_table_name_(new_table_name),
        index_name_(index_name),
        index_field_meta_(field_meta)
  {}
  StmtType type() const override { return StmtType::ALTER_TABLE; }

public:
  static RC create(Db *db, AlterSqlNode &alter_sql, Stmt *&stmt);

public:
  Table                   *table() const { return table_; }
  AlterType                alter_type() const { return alter_type_; }
  const string            &new_attribute_name() const { return new_attribute_name_; }
  AttrInfoSqlNode          attr_info() const { return attr_info_; }
  const string            &new_table_name() const { return new_table_name_; }
  const string            &index_name() const { return index_name_; }
  const vector<FieldMeta> &index_field_meta() const { return index_field_meta_; }

private:
  Table          *table_ = nullptr;
  AlterType       alter_type_;
  AttrInfoSqlNode attr_info_;
  string          new_attribute_name_;
  string          new_table_name_;
  // for fulltext index
  string            index_name_;
  vector<FieldMeta> index_field_meta_;
};
