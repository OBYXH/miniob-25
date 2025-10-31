#include "sql/parser/parse_defs.h"
#/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
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

#include "sql/executor/alter_table_executor.h"

#include "common/log/log.h"
#include "event/session_event.h"
#include "event/sql_event.h"
#include "session/session.h"
#include "sql/stmt/alter_table_stmt.h"
#include "storage/db/db.h"

RC AlterTableExecutor::execute(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  ASSERT(stmt->type() == StmtType::ALTER_TABLE,
      "alter table executor can not run this command: %d", static_cast<int>(stmt->type()));
  AlterTableStmt *alter_table_stmt = static_cast<AlterTableStmt *>(stmt);
  switch (alter_table_stmt->alter_type()) {
    case AlterType::ALTER_ADD: {
      RC rc = alter_add(sql_event);
      return rc;
    } break;
    case AlterType::ALTER_DROP: {
      RC rc = alter_drop(sql_event);
      return rc;
    } break;
    case AlterType::ALTER_CHANGE: {
      RC rc = alter_change(sql_event);
      return rc;
    } break;
    case AlterType::ALTER_RENAME: {
      RC rc = alter_rename(sql_event);
      return rc;
    } break;
    default: {
      LOG_WARN("unsupported alter type: %d", static_cast<int>(alter_table_stmt->alter_type()));
      return RC::UNIMPLEMENTED;
    }
  }
  return RC::SUCCESS;
}

RC AlterTableExecutor::alter_add(SQLStageEvent *sql_event)
{
  AlterTableStmt *alter_table_stmt = static_cast<AlterTableStmt *>(sql_event->stmt());
  Table          *table            = alter_table_stmt->table();
  AttrInfoSqlNode add_node         = alter_table_stmt->attr_info();
  RC              rc               = table->add_column(add_node, sql_event->session_event()->session()->current_trx());
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to add column %s to table %s, rc=%d", add_node.name.c_str(), table->name(), rc);
    return rc;
  }
  return RC::SUCCESS;
}

RC AlterTableExecutor::alter_drop(SQLStageEvent *sql_event)
{
  AlterTableStmt *alter_table_stmt = static_cast<AlterTableStmt *>(sql_event->stmt());
  Table          *table            = alter_table_stmt->table();
  AttrInfoSqlNode add_node         = alter_table_stmt->attr_info();
  RC              rc               = table->drop_column(add_node, sql_event->session_event()->session()->current_trx());
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to drop column %s from table %s, rc=%d", add_node.name.c_str(), table->name(), rc);
    return rc;
  }
  return RC::SUCCESS;
}

RC AlterTableExecutor::alter_change(SQLStageEvent *sql_event)
{
  AlterTableStmt *alter_table_stmt   = static_cast<AlterTableStmt *>(sql_event->stmt());
  Table          *table              = alter_table_stmt->table();
  AttrInfoSqlNode add_node           = alter_table_stmt->attr_info();
  string          new_attribute_name = alter_table_stmt->new_attribute_name();
  RC rc = table->change_column(add_node, new_attribute_name, sql_event->session_event()->session()->current_trx());
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to change column %s to %s in table %s, rc=%d", add_node.name.c_str(),new_attribute_name.c_str(), table->name(), rc);
    return rc;
  }
  return RC::SUCCESS;
}

RC AlterTableExecutor::alter_rename(SQLStageEvent *sql_event)
{
  AlterTableStmt *alter_table_stmt = static_cast<AlterTableStmt *>(sql_event->stmt());
  Table          *table            = alter_table_stmt->table();
  string          new_table_name   = alter_table_stmt->new_table_name();
  RC rc = table->rename_table(new_table_name.c_str(), sql_event->session_event()->session()->current_trx());
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to change table name from %s to %s, rc=%d",  table->name(),new_table_name.c_str(), rc);
    return rc;
  }
  return RC::SUCCESS;
}