/* Copyright (c) 2021OceanBase and/or its affiliates. All rights reserved.
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

#include "sql/stmt/insert_stmt.h"
#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

InsertStmt::InsertStmt(Table *table, const Value *values, int value_amount)
    : table_(table), values_(values), value_amount_(value_amount)
{}

RC InsertStmt::create(Db *db, const InsertSqlNode &inserts, Stmt *&stmt)
{
  const char *table_name = inserts.relation_name.c_str();
  if (nullptr == db || nullptr == table_name || inserts.values.empty()) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, value_num=%d",
        db, table_name, static_cast<int>(inserts.values.size()));
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC_WITH_LOCATION(RC::SCHEMA_TABLE_NOT_EXIST, "");
  }

  if (table->is_view()) {
    auto *view = static_cast<View *>(table);
    if (!view->is_insert_allowed()) {
      LOG_WARN("the target table(view) of the INSERT is not insertable-into");
      return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
    }
  }
  
  // check the fields number
  const Value     *values     = inserts.values.data();
  const int        value_num  = static_cast<int>(inserts.values.size());
  const TableMeta &table_meta = table->table_meta();
  const int        field_num  = table_meta.field_num() - table_meta.sys_field_num();

  if (!table->is_view() || (table->is_view() && inserts.attrs_name.empty())) {
    // 不是视图，或者是没有指定 field list 的视图插入操作。
    if (field_num != value_num) {
      LOG_WARN("schema mismatch. value num=%d, field num in schema=%d", value_num, field_num);
      return RC::SCHEMA_FIELD_MISSING;
    }
  } else {
    // 指定了 field list 的视图的视图插入操作，需要检查 attrs_name 是否和 value_num 匹配
    if (inserts.attrs_name.size() != value_num) {
      LOG_WARN("schema mismatch. value num=%d, field num in schema=%d", value_num, field_num);
      return RC::SCHEMA_FIELD_MISSING;
    }
  }

  // everything alright
  auto insert_stmt = new InsertStmt(table, values, value_num);
  insert_stmt->set_attrs_name(inserts.attrs_name);
  stmt = insert_stmt;
  return RC::SUCCESS;
}
