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
#include "common/value.h"
#include "storage/db/db.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/field/field_meta.h"
#include <utility>
#include <vector>

UpdateStmt::UpdateStmt(
    Table *table, vector<const Value *> values, vector<FieldMeta> field_metas, FilterStmt *filter_stmt)
    : table_(table), values_(std::move(values)), field_metas_(std::move(field_metas)), filter_stmt_(filter_stmt)
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

  unordered_map<std::string, Table *> table_map;
  table_map.insert(pair<string, Table *>(string(table_name), table));

  FilterStmt *filter_stmt = nullptr;
  RC          rc = FilterStmt::create(db, table, &table_map, update.conditions, filter_stmt, FilterStmt::Type::WHERE);
  if (rc != RC::SUCCESS) {
    return rc;
  }
  std::vector<const Value *> values;
  std::vector<FieldMeta>     field_metas;
  for (auto &update_field : update.update_list) {
    auto field_meta = table->table_meta().field(update_field.attribute_name.c_str());
    auto value      = &update_field.value;
    if (field_meta == nullptr) {
      LOG_WARN("no such field. table=%s, field=%s", table_name, update_field.attribute_name.c_str());
      return RC::SCHEMA_FIELD_NOT_EXIST;
    }

    if (field_meta->type() == AttrType::VECTORS) {
      ASSERT(field_meta->len()==value->length(), " field len doesn't match cell len , field_meta->len=%d, cell.length=%d", field_meta->len(), value->length());
    }
    field_metas.push_back(*field_meta);
    values.push_back(value);
  }
  stmt = new UpdateStmt(table, std::move(values), std::move(field_metas), filter_stmt);
  return rc;
}
