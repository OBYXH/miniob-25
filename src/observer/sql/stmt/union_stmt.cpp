#include "sql/stmt/union_stmt.h"
#include "sql/stmt/select_stmt.h"


RC UnionStmt::create(Db *db, UnionSqlNode &union_sql, Stmt *&stmt) {
  UnionStmt *union_stmt = new UnionStmt();
  
  // 1. 创建所有子 SELECT 语句
  if (union_sql.unions.empty()) {
    LOG_WARN("union must have at least one select statement");
    delete union_stmt;
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }

  for (auto &union_unit : union_sql.unions) {
    Stmt *select_stmt = nullptr;
    RC rc = SelectStmt::create(db, union_unit.selection, select_stmt);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to create select stmt for union. rc=%s", strrc(rc));
      delete union_stmt;
      return rc;
    }
    
    union_stmt->select_stmts_.emplace_back(static_cast<SelectStmt*>(select_stmt));
    union_stmt->union_types_.push_back(union_unit.union_type);
  }

  // 2. 验证所有 SELECT 的列数相同
  size_t expected_columns = union_stmt->select_stmts_[0]->query_expressions().size();
  for (size_t i = 1; i < union_stmt->select_stmts_.size(); i++) {
    size_t actual_columns = union_stmt->select_stmts_[i]->query_expressions().size();
    if (actual_columns != expected_columns) {
      LOG_WARN("union select statements have different column counts: %zu vs %zu", 
               expected_columns, actual_columns);
      delete union_stmt;
      return RC::SCHEMA_UNION_DISMATCH;
    }
  }

  stmt = union_stmt;
  return RC::SUCCESS;
}