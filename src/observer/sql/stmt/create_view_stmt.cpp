#include "sql/stmt/create_view_stmt.h"
#include "common/log/log.h"
#include "sql/stmt/create_table_stmt.h"
#include "event/sql_debug.h"

#include "sql/parser/parse_defs.h"
#include "sql/stmt/select_stmt.h"

RC CreateViewStmt::create(Db *db, CreateViewSqlNode &create_view, Stmt *&stmt) {
  // create table select
  SelectStmt *select_stmt = nullptr;
  Stmt *stmt_ = nullptr;
  if (create_view.sub_select != nullptr) {
    // create table select
    RC rc = Stmt::create_stmt(db, *create_view.sub_select, stmt_);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create stmt. rc=%d:%s", rc, strrc(rc));
      return rc;
    }
    // cast to SelectStmt
    select_stmt = static_cast<SelectStmt *>(stmt_);
  }
  stmt = new CreateViewStmt(create_view.view_name);
  
  if (create_view.sub_select != nullptr) {
    auto *create_view_stmt = static_cast<CreateViewStmt *>(stmt);
    create_view_stmt->set_select_stmt(select_stmt);
    create_view_stmt->set_query_fields(select_stmt->get_query_fields());
    create_view_stmt->set_view_definition(create_view.description);
  }

  sql_debug("create table statement: table name %s", create_view.view_name.c_str());
  
  return RC::SUCCESS;
}