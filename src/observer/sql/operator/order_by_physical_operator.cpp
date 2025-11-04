#include "order_by_physical_operator.h"
#include "common/log/log.h"

OrderByPhysicalOperator::OrderByPhysicalOperator(vector<OrderBySqlNode> order_by, vector<Expression *> exprs, int limit)
    : order_by_(std::move(order_by)), exprs_(std::move(exprs)), limit_(limit)
{
  vector<Expression *> expressions;
  expressions.reserve(exprs_.size());
  for (auto &expr : exprs_) {
    expressions.push_back(expr);
  }
  tuple_.init(expressions);
}

RC OrderByPhysicalOperator::fetch_and_sort_with_heap()
{
  RC rc = RC::SUCCESS;

  // ✅ 完全复用 fetch_and_sort_tables 的比较器
  auto cmp = [this](const pair<vector<Value>, vector<Value>> &cells_a,
                    const pair<vector<Value>, vector<Value>> &cells_b) -> bool {
    auto  order_size   = order_by_.size();
    auto &order_line_a = cells_a.first;
    auto &order_line_b = cells_b.first;

    for (size_t i = 0; i < order_size; i++) {
      auto a      = order_line_a[i];
      auto b      = order_line_b[i];
      auto result = a.compare(b);
      auto is_asc = order_by_[i].is_asc;
      if (result < 0) {
        return is_asc;
      } else if (result > 0) {
        return !is_asc;
      }
    }
    return false;
  };

  // ✅ 关键：priority_queue 默认是最大堆
  // 比较器返回 true 表示第一个参数优先级更低（会沉到堆底）
  // 我们需要保留排序后最前面的 k 个，所以堆顶应该是第 k 个元素
  std::priority_queue<
      pair<vector<Value>, vector<Value>>,
      vector<pair<vector<Value>, vector<Value>>>,
      decltype(cmp)  // ✅ 直接使用 cmp
  > heap(cmp);

  int rows = 0;
  // ✅ 遍历数据
  while (RC::SUCCESS == (rc = children_[0]->next())) {
    rows++;
    vector<Value> order_by_line;
    for (auto &[expr, asc] : order_by_) {
      Value cell;
      rc = expr->get_value(*children_[0]->current_tuple(), cell);
      if (OB_FAIL(rc)) {
        return rc;
      }
      order_by_line.emplace_back(cell);
    }

    vector<Value> field_line;
    for (auto &expr : tuple_.exprs()) {
      Value cell;
      rc = expr->get_value(*children_[0]->current_tuple(), cell);
      if (OB_FAIL(rc)) {
        return rc;
      }
      field_line.emplace_back(cell);
    }

    auto row = make_pair(order_by_line, field_line);

    if (heap.size() < static_cast<size_t>(limit_)) {
      heap.push(row);
    } else {
      // ✅ 关键：如果新元素比堆顶小（应该排在堆顶前面）
      if (cmp(row, heap.top())) {
        heap.pop();
        heap.push(row);
      }
    }
  }
  LOG_WARN("Total rows fetched for ORDER BY: %d", rows);

  rc = RC::SUCCESS;

  // ✅ 用临时数组收集结果
  vector<vector<Value>> temp;
  temp.reserve(heap.size());
  while (!heap.empty()) {
    temp.push_back(heap.top().second);
    heap.pop();
  }

  // ✅ 反转
  std::reverse(temp.begin(), temp.end());

  values_ = std::move(temp);
  it_ = values_.begin();
  return rc;
}

RC OrderByPhysicalOperator::fetch_and_sort_tables()
{
  if (limit_ > 0 && limit_ <= 100) {  // 阈值可以根据实际情况调整
    return fetch_and_sort_with_heap();
  }
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
    return RC::INTERNAL;
  }
  rc = children_[0]->open(trx);
  if (OB_FAIL(rc)) {
    return rc;
  }
  rc = fetch_and_sort_tables();
  return rc;
}

RC OrderByPhysicalOperator::next()
{
  RC rc = RC::SUCCESS;
  if (it_ == values_.end()) {
    return RC::RECORD_EOF;
  }

  const vector<Value> &value = *it_;
  tuple_.set_cells(value);
  it_++;
  return rc;
}

RC OrderByPhysicalOperator::close() { return children_[0]->close(); }

Tuple *OrderByPhysicalOperator::current_tuple() { return &tuple_; }