#include "sql/operator/union_physical_operator.h"
#include "common/log/log.h"

RC UnionPhysicalOperator::open(Trx *trx)
{
  RC rc = RC::SUCCESS;

  // 1. 执行所有子算子，收集结果
  rc = execute_child_operators(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to execute child operators. rc=%s", strrc(rc));
    return rc;
  }

  // 2. 如果需要去重（有任何 UNION 而非 UNION ALL）
  bool need_dedup = false;
  for (char union_type : union_types_) {
    if (union_type == 1) {  // 1 表示 UNION（去重）
      need_dedup = true;
      break;
    }
  }

  if (need_dedup) {
    rc = remove_duplicates();
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to remove duplicates. rc=%s", strrc(rc));
      return rc;
    }
  }

  current_index_ = 0;
  return RC::SUCCESS;
}

RC UnionPhysicalOperator::execute_child_operators(Trx *trx)
{
  if (children_.empty()) {
    LOG_WARN("union operator has no children");
    return RC::INTERNAL;
  }

  // 遍历所有子算子
  for (auto &child : children_) {
    RC rc = child->open(trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to open child operator. rc=%s", strrc(rc));
      return rc;
    }

    // 获取子算子的所有结果
    while ((rc = child->next()) == RC::SUCCESS) {
      Tuple *tuple = child->current_tuple();
      if (tuple == nullptr) {
        LOG_WARN("child operator returned null tuple");
        child->close();
        return RC::INTERNAL;
      }
      
      // 将 tuple 的所有 cell 提取出来
      TupleData tuple_data;
      for (int i = 0; i < tuple->cell_num(); i++) {
        Value value;
        rc = tuple->cell_at(i, value);
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to get cell at %d. rc=%s", i, strrc(rc));
          child->close();
          return rc;
        }
        tuple_data.values.push_back(value);
      }
      
      result_tuples_.push_back(std::move(tuple_data));
    }

    child->close();
    
    // 子算子正常结束应该返回 RECORD_EOF
    if (rc != RC::RECORD_EOF) {
      LOG_WARN("child operator returned unexpected error. rc=%s", strrc(rc));
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC UnionPhysicalOperator::remove_duplicates()
{
  // 使用 unordered_set 进行哈希去重
  std::unordered_set<TupleData, TupleDataHash> unique_tuples;
  std::vector<TupleData> dedup_result;

  for (auto &tuple : result_tuples_) {
    // insert 返回 pair<iterator, bool>，second 为 true 表示插入成功（之前不存在）
    if (unique_tuples.insert(tuple).second) {
      dedup_result.push_back(tuple);
    }
  }

  // 用去重后的结果替换原结果
  result_tuples_ = std::move(dedup_result);
  
  LOG_TRACE("union removed %zu duplicate rows", unique_tuples.size() - dedup_result.size());
  return RC::SUCCESS;
}

RC UnionPhysicalOperator::next()
{
  if (current_index_ >= result_tuples_.size()) {
    return RC::RECORD_EOF;
  }

  // 设置当前 tuple
  current_tuple_.set_cells(result_tuples_[current_index_].values);
  current_index_++;
  
  return RC::SUCCESS;
}

RC UnionPhysicalOperator::close()
{
  // 清理所有子算子
  for (auto &child : children_) {
    child->close();
  }
  
  result_tuples_.clear();
  current_index_ = 0;
  return RC::SUCCESS;
}

Tuple *UnionPhysicalOperator::current_tuple()
{
  return &current_tuple_;
}