#include "sql/operator/physical_operator.h"
#include "sql/expr/tuple.h"
#include "sql/operator/external_sorter.h"
#include <queue>

class OrderByPhysicalOperator : public PhysicalOperator
{
public:
  OrderByPhysicalOperator(std::vector<OrderBySqlNode> order_by, std::vector<Expression *> exprs, int limit);

  virtual ~OrderByPhysicalOperator() = default;

  PhysicalOperatorType type() const override { return PhysicalOperatorType::ORDER_BY; }

  std::vector<OrderBySqlNode> &order_by() { return order_by_; }

  std::vector<Expression *> &exprs() { return exprs_; }

  RC fetch_and_sort_tables();
  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;
  int limit() {
    return limit_;
  }

  Tuple *current_tuple() override;
  RC fetch_and_sort_with_heap();

private:
  std::vector<OrderBySqlNode> order_by_;

  /// 在 create order by stmt 之前提取 select clause 后的 field_expr (非agg_expr 中的) 和 agg_expr
  std::vector<Expression *> exprs_;

  std::vector<std::vector<Value>> values_;

  std::vector<std::vector<Value>>::iterator it_;
  SplicedTuple                              tuple_;

  std::unique_ptr<ExternalSorter> sorter_;

  static constexpr size_t MAX_MEMORY_BYTES = 200 * 1024 * 1024;  // 100MB

  int limit_;
};