#pragma once

#include "sql/operator/logical_operator.h"
#include <vector>

/**
 * @brief UNION 逻辑算子
 */
class UnionLogicalOperator : public LogicalOperator {
public:
  UnionLogicalOperator(std::vector<char> union_types) 
    : union_types_(std::move(union_types)) {}
  
  virtual ~UnionLogicalOperator() = default;

  LogicalOperatorType type() const override { 
    return LogicalOperatorType::UNION; 
  }

  const std::vector<char> &union_types() const { 
    return union_types_; 
  }

private:
  std::vector<char> union_types_;  // 每个 UNION 的类型（0=ALL, 1=DISTINCT）
};