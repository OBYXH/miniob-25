#include "sql/operator/physical_operator.h"
#include "sql/expr/tuple.h"

class OrderByPhysicalOperator : public PhysicalOperator
{
public:
  OrderByPhysicalOperator(std::vector<OrderBySqlNode> order_by, std::vector<Expression *> exprs);

  virtual ~OrderByPhysicalOperator() = default;

  PhysicalOperatorType type() const override { return PhysicalOperatorType::ORDER_BY; }

  std::vector<OrderBySqlNode> &order_by() { return order_by_; }

  std::vector<Expression *> &exprs() { return exprs_; }

  RC fetch_and_sort_tables();
  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override;

private:
  std::vector<OrderBySqlNode> order_by_;

  /// 在 create order by stmt 之前提取 select clause 后的 field_expr (非agg_expr 中的) 和 agg_expr
  std::vector<Expression *> exprs_;

  std::vector<std::vector<Value>> values_;

  std::vector<std::vector<Value>>::iterator it_;
  SplicedTuple                              tuple_;
};