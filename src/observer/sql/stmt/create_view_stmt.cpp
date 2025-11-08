#include "sql/stmt/create_view_stmt.h"
#include "common/log/log.h"
#include "sql/stmt/create_table_stmt.h"
#include "event/sql_debug.h"

#include "sql/parser/parse_defs.h"
#include "sql/stmt/select_stmt.h"

bool check_is_update_allowed(SelectStmt *select_stmt) {
    // 检查是否有聚合函数
    if (select_stmt->has_aggr()) {
        return false;
    }
    
    return true;
}

bool check_is_insert_allowed(SelectStmt *select_stmt) {
    // 检查是否有聚合函数
    if (select_stmt->has_aggr()) {
        return false;
    }
    // 检查是否有算术表达式
    if (select_stmt->has_arithmatic()) {
        return false;
    }
    return true;
}

bool check_is_delete_allowed(SelectStmt *select_stmt) {
    // 检查是否有聚合函数
    if (select_stmt->has_aggr()) {
        return false;
    }
    
    // 检查是否有join
    if (select_stmt->has_join()) {
        return false;
    }
    return true;
}

RC CreateViewStmt::create(Db *db, CreateViewSqlNode &create_view, Stmt *&stmt) {
  // create table select
  SelectStmt *select_stmt = nullptr;
  Stmt *stmt_ = nullptr;

  if (create_view.sub_select == nullptr) {
    LOG_WARN("create view must have a sub select");
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }

  RC rc = Stmt::create_stmt(db, *create_view.sub_select, stmt_);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create stmt. rc=%d:%s", rc.to_int(), strrc(rc));
    return rc;
  }
  // cast to SelectStmt
  select_stmt = static_cast<SelectStmt *>(stmt_);

  auto query_fields = select_stmt->get_query_fields();

  if (!create_view.attrs_name.empty() && query_fields.size() != create_view.attrs_name.size()) {
    LOG_WARN("select query expr num count doesn't match attr count");
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }

  stmt = new CreateViewStmt(create_view.view_name, create_view.attrs_name);
  
  auto *create_view_stmt = static_cast<CreateViewStmt *>(stmt);
  // 预判断 view 的各操作下可变性
  create_view_stmt->set_is_update_allowed(check_is_update_allowed(select_stmt));
  create_view_stmt->set_is_insert_allowed(check_is_insert_allowed(select_stmt));
  create_view_stmt->set_is_delete_allowed(check_is_delete_allowed(select_stmt));
  
  create_view_stmt->set_select_stmt(select_stmt);
  create_view_stmt->set_query_fields(query_fields);
  // 检查 duplicate column name
  if (create_view_stmt->has_duplicate_column_name()) {
    LOG_WARN("duplicate column name in view definition(ERROR 1060)");
    return RC::INVALID_ARGUMENT;
  }
  create_view_stmt->set_view_definition(create_view.description);

  sql_debug("create view statement: view name %s", create_view.view_name.c_str());
  
  return RC::SUCCESS;
}

bool CreateViewStmt::has_duplicate_column_name() {
    std::unordered_map<std::string, int> column_name_map;
    for (auto &field : query_fields_meta_) {
        if (column_name_map.find(field.name()) != column_name_map.end()) {
            return true;
        }
        column_name_map[field.name()] = 1;
    }
    return false;
}