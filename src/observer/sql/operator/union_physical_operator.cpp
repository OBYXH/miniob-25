#include "sql/operator/union_physical_operator.h"
#include "common/log/log.h"

RC UnionPhysicalOperator::open(Trx *trx)
{
  RC rc = RC::SUCCESS;

  // 0. 验证所有子算子的 schema 兼容性
  rc = validate_schema_compatibility();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to validate schema compatibility. rc=%s", strrc(rc));
    return rc;
  }

  // 1. 执行所有子算子，收集结果
  rc = execute_child_operators(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to execute child operators. rc=%s", strrc(rc));
    return rc;
  }

  current_index_ = 0;
  return RC::SUCCESS;
}

RC UnionPhysicalOperator::validate_schema_compatibility()
{
  if (children_.empty()) {
    LOG_WARN("union operator has no children");
    return RC::INTERNAL;
  }

  // 获取第一个子算子的 schema 作为基准
  TupleSchema base_schema;
  RC rc = children_[0]->tuple_schema(base_schema);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get schema from first child. rc=%s", strrc(rc));
    return rc;
  }

  int base_cell_num = base_schema.cell_num();
  if (base_cell_num == 0) {
    LOG_WARN("first child has empty schema");
    return RC::INTERNAL;
  }

  // 验证后续子算子的 schema 与第一个子算子兼容
  for (size_t i = 1; i < children_.size(); i++) {
    TupleSchema child_schema;
    rc = children_[i]->tuple_schema(child_schema);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get schema from child %zu. rc=%s", i, strrc(rc));
      return rc;
    }

    // 检查列数是否一致
    if (child_schema.cell_num() != base_cell_num) {
      LOG_WARN("UNION queries have different column counts: first has %d, child %zu has %d",
               base_cell_num, i, child_schema.cell_num());
      return RC::SCHEMA_FIELD_MISSING;
    }
  }

  return RC::SUCCESS;
}

RC UnionPhysicalOperator::execute_child_operators(Trx *trx)
{
  if (children_.empty()) {
    LOG_WARN("union operator has no children");
    return RC::INTERNAL;
  }

  bool first_child = true;
  std::vector<AttrType> base_types;  // 存储第一个子算子的列类型

  // 遍历所有子算子
  for (size_t child_idx = 0; child_idx < children_.size(); child_idx++) {
    auto &child = children_[child_idx];
    RC rc = child->open(trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to open child operator %zu. rc=%s", child_idx, strrc(rc));
      return rc;
    }

    bool first_row = true;
    
    char union_type = union_types_[child_idx];

    // 临时存储当前子查询的结果
    std::vector<TupleData> current_child_results;

    // 获取子算子的所有结果
    while ((rc = child->next()) == RC::SUCCESS) {
      Tuple *tuple = child->current_tuple();
      if (tuple == nullptr) {
        LOG_WARN("child operator returned null tuple");
        child->close();
        return RC::INTERNAL;
      }
      
      int cell_num = tuple->cell_num();

      // 从第一个子算子的第一行获取类型信息
      if (first_child && first_row) {
        base_types.reserve(cell_num);
        
        for (int i = 0; i < cell_num; i++) {
          Value value;
          rc = tuple->cell_at(i, value);
          if (rc != RC::SUCCESS) {
            LOG_WARN("failed to get cell at %d. rc=%s", i, strrc(rc));
            child->close();
            return rc;
          }
          base_types.push_back(value.attr_type());
        }
        first_row = false;
      } 
      // 验证后续子算子的数据类型
      else if (first_row && !first_child) {
        if (cell_num != static_cast<int>(base_types.size())) {
          LOG_WARN("UNION child %zu has different column count: expected %zu, got %d",
                   child_idx, base_types.size(), cell_num);
          child->close();
          return RC::SCHEMA_FIELD_MISSING;
        }

        for (int i = 0; i < cell_num; i++) {
          Value value;
          rc = tuple->cell_at(i, value);
          if (rc != RC::SUCCESS) {
            LOG_WARN("failed to get cell at %d. rc=%s", i, strrc(rc));
            child->close();
            return rc;
          }

          // 检查数据类型是否兼容
          if (value.attr_type() != base_types[i] && 
              value.attr_type() != AttrType::NULLS && 
              base_types[i] != AttrType::NULLS) {
            LOG_WARN("UNION child %zu column %d has incompatible type: expected %s, got %s",
                     child_idx, i, 
                     attr_type_to_string(base_types[i]),
                     attr_type_to_string(value.attr_type()));
            child->close();
            return RC::SCHEMA_FIELD_TYPE_MISMATCH;
          }
        }
        first_row = false;
      }
      
      // 将 tuple 的所有 cell 提取出来
      TupleData tuple_data;
      for (int i = 0; i < cell_num; i++) {
        Value value;
        rc = tuple->cell_at(i, value);
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to get cell at %d. rc=%s", i, strrc(rc));
          child->close();
          return rc;
        }
        tuple_data.values.push_back(value);
      }
      
      current_child_results.push_back(tuple_data);
    }

    child->close();
    first_child = false;
    
    // 子算子正常结束应该返回 RECORD_EOF
    if (rc != RC::RECORD_EOF) {
      LOG_WARN("child operator returned unexpected error. rc=%s", strrc(rc));
      return rc;
    }

    // 根据子查询索引和 UNION 类型决定处理方式
    if (child_idx == 0) {
      // 第一个子查询：直接添加所有结果
      result_tuples_ = std::move(current_child_results);
    } else if (union_type == 0) {
      // UNION ALL：直接追加当前子查询的结果
      result_tuples_.insert(result_tuples_.end(), 
                           current_child_results.begin(), 
                           current_child_results.end());
    } else {
      // UNION：先追加当前子查询的结果，然后对整体去重
      result_tuples_.insert(result_tuples_.end(), 
                           current_child_results.begin(), 
                           current_child_results.end());
      
      // 对当前累积的所有结果去重
      rc = remove_duplicates();
      if (rc != RC::SUCCESS) {
        LOG_WARN("failed to remove duplicates. rc=%s", strrc(rc));
        return rc;
      }
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

  size_t removed_count = result_tuples_.size() - dedup_result.size();
  
  // 用去重后的结果替换原结果
  result_tuples_ = std::move(dedup_result);
  
  LOG_TRACE("union removed %zu duplicate rows", removed_count);
  return RC::SUCCESS;
}

RC UnionPhysicalOperator::next()
{
  if (current_index_ >= result_tuples_.size()) {
    return RC::RECORD_EOF;
  }

  // 设置当前 tuple 的数据
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

RC UnionPhysicalOperator::tuple_schema(TupleSchema &schema) const
{
  if (children_.empty()) {
    LOG_WARN("union operator has no children");
    return RC::INTERNAL;
  }
  
  // 直接使用第一个子算子的 schema
  return children_[0]->tuple_schema(schema);
}