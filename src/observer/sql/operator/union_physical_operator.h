#pragma once

#include "sql/operator/physical_operator.h"
#include "storage/record/record.h"
#include "common/value.h"
#include "sql/expr/tuple_cell.h"
#include "sql/expr/tuple.h"
#include <vector>
#include <unordered_set>

/**
 * @brief UNION 物理算子
 * @ingroup PhysicalOperator
 * 
 * 实现逻辑：
 * 1. 第一个 SELECT 的结果全部加入
 * 2. 从第二个 SELECT 开始，根据 UNION 类型：
 *    - UNION ALL：直接追加所有结果
 *    - UNION：追加结果后对截止目前的所有数据进行去重
 * 3. 迭代返回结果
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
  RC validate_schema_compatibility();
  RC remove_duplicates();  // 对当前累积的结果去重
  
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
  size_t current_index_ = 0;                   // 当前迭代位置
  ValueListTuple current_tuple_;               // 当前返回的 tuple
};