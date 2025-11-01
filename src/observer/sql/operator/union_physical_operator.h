#pragma once

#include "sql/operator/physical_operator.h"
#include "storage/record/record.h"
#include "common/value.h"
#include "sql/expr/tuple_cell.h"
#include <vector>
#include <unordered_set>

/**
 * @brief UNION 物理算子
 * @ingroup PhysicalOperator
 * 
 * 实现逻辑：
 * 1. 依次执行所有子算子
 * 2. 将结果存入内存临时表
 * 3. 如果是 UNION（非 ALL），去重
 * 4. 迭代返回结果
 */
class UnionPhysicalOperator : public PhysicalOperator
{
public:
  UnionPhysicalOperator(std::vector<char> union_types) 
    : union_types_(std::move(union_types)) {}
  
  virtual ~UnionPhysicalOperator() = default;

  PhysicalOperatorType type() const override
  {
    return PhysicalOperatorType::UNION;
  }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override;

  RC tuple_schema(TupleSchema &schema) const override;

private:
  RC execute_child_operators(Trx *trx);
  RC remove_duplicates();
  RC validate_schema_compatibility();  // 验证 schema 兼容性
  
  /**
   * @brief 用于存储单行数据的结构
   */
  struct TupleData {
    std::vector<Value> values;
    
    bool operator==(const TupleData &other) const
    {
      if (values.size() != other.values.size()) {
        return false;
      }
      for (size_t i = 0; i < values.size(); i++) {
        if (values[i].compare(other.values[i]) != 0) {
          return false;
        }
      }
      return true;
    }
  };

  /**
   * @brief 哈希函数，用于 unordered_set 去重
   */
  struct TupleDataHash {
    size_t operator()(const TupleData &tuple) const
    {
      size_t hash = 0;
      for (const auto &value : tuple.values) {
        // 简单的哈希组合
        hash ^= std::hash<std::string>{}(value.to_string()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      }
      return hash;
    }
  };

private:
  std::vector<char> union_types_;              // 每个 UNION 的类型（0=ALL, 1=DISTINCT）
  std::vector<TupleData> result_tuples_;       // 存储所有结果行
  std::vector<TupleCellSpec> tuple_specs_;     // 存储 tuple 的 schema 信息
  size_t current_index_ = 0;                   // 当前迭代位置
  ValueListTuple current_tuple_;               // 当前返回的 tuple
};