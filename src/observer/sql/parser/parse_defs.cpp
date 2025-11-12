#include "sql/parser/parse_defs.h"
#include "sql/expr/expression.h"

// ConditionSqlNode 的实现
ConditionSqlNode::ConditionSqlNode() = default;

ConditionSqlNode::~ConditionSqlNode() = default;

ConditionSqlNode::ConditionSqlNode(ConditionSqlNode&& other) noexcept 
  : comp(other.comp),
    left(std::move(other.left)),
    right(std::move(other.right)),
    conjunction_type(other.conjunction_type) {
}

ConditionSqlNode& ConditionSqlNode::operator=(ConditionSqlNode&& other) noexcept {
  if (this != &other) {
    comp = other.comp;
    left = std::move(other.left);
    right = std::move(other.right);
    conjunction_type = other.conjunction_type;
  }
  return *this;
}
