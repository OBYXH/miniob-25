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
// Created by Wangyunlai on 2023/6/13.
//

#include "sql/stmt/show_index_stmt.h"
#include "event/sql_debug.h"
#include "storage/db/db.h"

RC ShowIndexStmt::create(Db *db, const ShowIndexSqlNode &show_index, Stmt *&stmt)
{
  if (db->find_table(show_index.relation_name.c_str()) == nullptr) {
    sql_debug("table %s not found", show_index.relation_name.c_str());
    return RC_WITH_LOCATION(RC::SCHEMA_TABLE_NOT_EXIST, "");
  }
  stmt = new ShowIndexStmt(show_index.relation_name);
  return RC::SUCCESS;
}
