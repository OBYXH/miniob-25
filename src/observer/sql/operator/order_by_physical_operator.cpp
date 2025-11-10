#include "order_by_physical_operator.h"

OrderByPhysicalOperator::OrderByPhysicalOperator(vector<OrderBySqlNode> order_by, vector<Expression *> exprs)
    : order_by_(std::move(order_by)), exprs_(std::move(exprs))
{
  vector<Expression *> expressions;
  expressions.reserve(exprs_.size());
  for (auto &expr : exprs_) {
    expressions.push_back(expr);
  }
  tuple_.init(expressions);
}

RC OrderByPhysicalOperator::fetch_and_sort_tables()
{
  RC rc = RC::SUCCESS;

  vector<pair<vector<Value>, vector<Value>>> order_and_field_line;

  while (RC::SUCCESS == (rc = children_[0]->next())) {
    // 获取 order by 字段的 values
    vector<Value> order_by_line;
    for (auto &[expr, asc] : order_by_) {
      Value cell;
      rc = expr->get_value(*children_[0]->current_tuple(), cell);
      if (OB_FAIL(rc)) {
        return rc;
      }
      order_by_line.emplace_back(cell);
    }

    // 获取 select 字段的 values
    vector<Value> field_line;
    for (auto &expr : tuple_.exprs()) {
      Value cell;
      rc = expr->get_value(*children_[0]->current_tuple(), cell);
      if (OB_FAIL(rc)) {
        return rc;
      }
      field_line.emplace_back(cell);
    }

    order_and_field_line.emplace_back(order_by_line, field_line);
  }

  rc = RC::SUCCESS;

  // consider null
  auto cmp = [this](const pair<vector<Value>, vector<Value>> &cells_a,
                 const pair<vector<Value>, vector<Value>>    &cells_b) -> bool {
    auto  order_size   = order_by_.size();
    auto &order_line_a = cells_a.first;
    auto &order_line_b = cells_b.first;
    assert(order_line_a.size() == order_size);
    assert(order_line_b.size() == order_size);
    assert(order_by_.size() == order_size);

    for (size_t i = 0; i < order_size; i++) {
      auto a      = order_line_a[i];
      auto b      = order_line_b[i];
      auto result = a.compare(b);
      auto is_asc = order_by_[i].is_asc;
      if (result < 0) {
        // a < b
        return is_asc;
      } else if (result > 0) {
        // a > 0
        return !is_asc;
      }
    }

    // order_line_a == order_line_b
    return false;
  };

  sort(order_and_field_line.begin(), order_and_field_line.end(), cmp);
  for (auto &[_, value] : order_and_field_line) {
    values_.push_back(value);
  }

  it_ = values_.begin();
  return rc;
}

RC OrderByPhysicalOperator::open(Trx *trx)
{
  RC rc = RC::SUCCESS;

  if (children_.size() != 1) {
    LOG_WARN("OrderByPhysicalOperator should have exactly one child");
    return RC_WITH_LOCATION(RC::INTERNAL, "");
  }

  PhysicalOperator *child = children_[0].get();
  if (outer_tuple != nullptr) {
    LOG_DEBUG("msg from order_by_phy_oper: we are in subquery");
    child->set_outer_tuple(outer_tuple);
  }

  rc = children_[0]->open(trx);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  vector<bool> ascs;
  for (auto &[expr, is_asc] : order_by_) {
    ascs.emplace_back(is_asc);
  }
  // 创建外排序器
  sorter_ = make_unique<ExternalSorter>(ascs, MAX_MEMORY_BYTES);

  // 从子算子读取所有数据并添加到sorter
  while (RC::SUCCESS == (rc = children_[0]->next())) {
    // 获取 order by 字段的 values
    vector<Value> order_by_values;
    order_by_values.reserve(order_by_.size());

    for (auto &[expr, is_asc] : order_by_) {
      Value cell;
      rc = expr->get_value(*children_[0]->current_tuple(), cell);
      if (rc != RC::SUCCESS) {
        LOG_WARN("Failed to get order by value");
        return rc;
      }
      order_by_values.push_back(std::move(cell));
    }

    // 获取 select 字段的 values
    vector<Value> result_values;
    result_values.reserve(tuple_.exprs().size());

    for (auto &expr : tuple_.exprs()) {
      Value cell;
      rc = expr->get_value(*children_[0]->current_tuple(), cell);
      if (rc != RC::SUCCESS) {
        LOG_WARN("Failed to get result value");
        return rc;
      }
      result_values.push_back(std::move(cell));
    }

    // 添加到sorter
    rc = sorter_->add_row(order_by_values, result_values);
    if (rc != RC::SUCCESS) {
      LOG_WARN("Failed to add row to sorter");
      return rc;
    }
  }

  if (rc != RC::RECORD_EOF) {
    LOG_WARN("Error reading from child operator");
    return rc;
  }

  // 完成数据添加，开始排序
  rc = sorter_->finish_add();
  if (rc != RC::SUCCESS) {
    LOG_WARN("Failed to finish sorting");
    return rc;
  }

  return RC::SUCCESS;
}

RC OrderByPhysicalOperator::next()
{
  if (!sorter_) {
    return RC_WITH_LOCATION(RC::INTERNAL, "");
  }

  vector<Value> result_values;
  RC            rc = sorter_->next(result_values);

  if (rc == RC::SUCCESS) {
    tuple_.set_cells(result_values);
  }

  return rc;
}

RC OrderByPhysicalOperator::close() { return children_[0]->close(); }

Tuple *OrderByPhysicalOperator::current_tuple() { return &tuple_; }