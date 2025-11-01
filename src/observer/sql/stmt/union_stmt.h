#pragma once

#include "common/sys/rc.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/stmt.h"

/**
 * @brief 表示Union语句
 * @ingroup Statement
 */
class UnionStmt : public Stmt
{
public:
  UnionStmt() = default;
  ~UnionStmt() = default;

  StmtType type() const override { return StmtType::UNION; }

public:
  static RC create(Db *db, UnionSqlNode &union_sql, Stmt *&stmt);

  const std::vector<std::unique_ptr<SelectStmt>> &select_stmts() const { 
    return select_stmts_; 
  }

  const std::vector<char> &union_types() const { 
    return union_types_; 
  }

private:
  vector<std::unique_ptr<SelectStmt>> select_stmts_;  // 所有 SELECT 语句
  vector<char> union_types_;  // 每个 UNION 的类型（0=UNION ALL, 1=UNION）
};