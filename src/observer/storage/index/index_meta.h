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
// Created by Wangyunlai on 2021/5/12.
//

#pragma once

#include "common/sys/rc.h"
#include "common/lang/string.h"
#include "sql/parser/parse_defs.h"
#include "storage/field/field_meta.h"

class TableMeta;
class FieldMeta;

namespace Json {
class Value;
}  // namespace Json

/**
 * @brief 描述一个索引
 * @ingroup Index
 * @details 一个索引包含了表的哪些字段，索引的名称等。
 * 如果以后实现了多种类型的索引，还需要记录索引的类型，对应类型的一些元数据等
 */
class IndexMeta
{
public:
  IndexMeta() = default;

  RC init(const char *name, IndexType index_type, const vector<FieldMeta> &fields, bool unique = false);

public:
  const char              *name() const { return name_.c_str(); }
  IndexType                index_type() const { return index_type_; }
  const vector<FieldMeta> &fields() const { return fields_; }
  const vector<int>       &fields_offset() const { return fields_offset_; }
  int                      fields_total_len() const { return fields_total_len_; }
  bool                     unique() const { return is_unique_; }

  void desc(ostream &os) const { os << to_string(); }

public:
  void      to_json(Json::Value &json_value) const;
  static RC from_json(const Json::Value &json_value, IndexMeta &index);
  string    to_string() const;
  char     *make_entry_from_record(const char *record) const;

protected:
  string            name_;                      // index's name
  IndexType         index_type_;                // index's type
  vector<FieldMeta> fields_;                    // fields included in this index
  vector<int>       fields_offset_;             // offsets of fields in the index record
  int               fields_total_len_ = 0;      // total length of all fields in this index
  bool              is_unique_        = false;  // whether the index is unique
};
