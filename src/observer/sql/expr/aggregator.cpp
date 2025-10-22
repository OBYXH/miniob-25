/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2024/05/29.
//

#include "sql/expr/aggregator.h"
#include "common/log/log.h"
#include "common/sys/rc.h"
#include "common/type/attr_type.h"
#include "common/value.h"

RC SumAggregator::accumulate(const Value &value)
{
  if (value.attr_type() == AttrType::NULLS) {
    return RC::SUCCESS;
  }

  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
    return RC::SUCCESS;
  }

  ASSERT(value.attr_type() == value_.attr_type(), "type mismatch. value type: %s, value_.type: %s", 
        attr_type_to_string(value.attr_type()), attr_type_to_string(value_.attr_type()));

  Value::add(value, value_, value_);
  return RC::SUCCESS;
}

RC SumAggregator::evaluate(Value &result)
{
  result = value_;
  return RC::SUCCESS;
}

RC CountAggregator::accumulate(const Value &value)
{
  if (value.attr_type() == AttrType::NULLS) {
    return RC::SUCCESS;
  }
  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_.set_int(1);
    return RC::SUCCESS;
  }
  Value result;
  result.set_type(AttrType::INTS);
  Value::add(value_, Value(1), result);
  value_.set_value(result);
  return RC::SUCCESS;
}

RC CountAggregator::evaluate(Value &result)
{
  result = value_;
  return RC::SUCCESS;
}

RC AvgAggregator::accumulate(const Value &value)
{
  if (value.attr_type() == AttrType::NULLS) {
    return RC::SUCCESS;
  }
  count_ += 1;
  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
    return RC::SUCCESS;
  }

  ASSERT(value.attr_type() == value_.attr_type(), "type mismatch. value type: %s, value_.type: %s", 
        attr_type_to_string(value.attr_type()), attr_type_to_string(value_.attr_type()));

  Value::add(value, value_, value_);
  return RC::SUCCESS;
}

RC AvgAggregator::evaluate(Value &result)
{
  RC rc = RC::SUCCESS;
  if (value_.attr_type() == AttrType::INTS) {
    rc = Value::cast_to(value_, AttrType::FLOATS, result);
  } else {
    result.set_value(value_);
  }

  rc = Value::divide(result, Value(count_), result);
  return rc;
}

RC MaxAggregator::accumulate(const Value &value)
{
  if (value.attr_type() == AttrType::NULLS) {
    return RC::SUCCESS;
  }

  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
    return RC::SUCCESS;
  }
  auto cmp = value_.compare(value);
  if (cmp < 0) {
    value_ = value;
  }
  return RC::SUCCESS;
}
RC MaxAggregator::evaluate(Value &result)
{
  result.set_value(value_);
  return RC::SUCCESS;
}

RC MinAggregator::accumulate(const Value &value)
{
  if (value.attr_type() == AttrType::NULLS) {
    return RC::SUCCESS;
  }

  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
    return RC::SUCCESS;
  }
  auto cmp = value_.compare(value);
  if (cmp > 0) {
    value_ = value;
  }
  return RC::SUCCESS;
}
RC MinAggregator::evaluate(Value &result)
{
  result.set_value(value_);
  return RC::SUCCESS;
}