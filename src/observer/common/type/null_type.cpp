/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */


#include "common/type/null_type.h"
#include "common/type/attr_type.h"
#include "common/value.h"
#include "common/log/log.h"
#include "common/lang/comparator.h"

int NullType::compare(const Value &left, const Value &right) const
{
  ASSERT(left.attr_type() == AttrType::INTS, "left type is not integer");
  ASSERT(right.attr_type() == AttrType::INTS || right.attr_type() == AttrType::FLOATS, "right type is not numeric");
  if (right.attr_type() == AttrType::INTS) {
    return common::compare_int((void *)&left.value_.int_value_, (void *)&right.value_.int_value_);
  } else if (right.attr_type() == AttrType::FLOATS) {
    float left_val  = left.get_float();
    float right_val = right.get_float();
    return common::compare_float((void *)&left_val, (void *)&right_val);
  }
  return INT32_MAX;
}

RC NullType::cast_to(const Value &val, AttrType type, Value &result) const
{
  result.set_null();
  return RC::SUCCESS;
}

RC NullType::add(const Value &left, const Value &right, Value &result) const
{
  result.set_int(0);
  result.set_type(AttrType::NULLS);
  return RC::SUCCESS;
}

RC NullType::subtract(const Value &left, const Value &right, Value &result) const
{
  result.set_int(0);
  result.set_type(AttrType::NULLS);
  return RC::SUCCESS;
}

RC NullType::multiply(const Value &left, const Value &right, Value &result) const
{
  result.set_int(0);
  result.set_type(AttrType::NULLS);
  return RC::SUCCESS;
}

RC NullType::negative(const Value &val, Value &result) const
{
  result.set_int(0);
  result.set_type(AttrType::NULLS);
  return RC::SUCCESS;
}

RC NullType::set_value_from_str(Value &val, const string &data) const
{
  val.set_null();
  return RC::SUCCESS;
}

RC NullType::to_string(const Value &val, string &result) const
{
  stringstream ss;
  ss << "NULL";
  result = ss.str();
  return RC::SUCCESS;
}