/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/lang/comparator.h"
#include "common/log/log.h"
#include "common/type/char_type.h"
#include "common/type/attr_type.h"
#include "common/utils.h"
#include "common/value.h"

int CharType::compare(const Value &left, const Value &right) const
{
  if (right.is_null()) {
    return 1;
  }
  ASSERT(left.attr_type() == AttrType::CHARS && right.attr_type() == AttrType::CHARS, "invalid type");
  return common::compare_string(
      (void *)left.value_.pointer_value_, left.length_, (void *)right.value_.pointer_value_, right.length_);
}

RC CharType::set_value_from_str(Value &val, const string &data) const
{
  val.set_string(data.c_str());
  return RC::SUCCESS;
}

RC CharType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch (type) {
    case AttrType::TEXTS: {
      if (val.length() > 65535) {
        LOG_WARN("Failed to cast char to text, data too long. data length=%d", val.length());
        return RC::DATA_TOO_LONG;
      }
      result.set_text(val.value_.pointer_value_, val.length_);
    } break;
    case AttrType::CHARS: {
      int date_val;
      RC  rc = parse_date(val.value_.pointer_value_, date_val);
      if (rc != RC::SUCCESS) {
        LOG_WARN("Failed to cast char to date, invalid date format. data=%s",
                 static_cast<const char *>(val.value_.pointer_value_));
        return rc;
      }
      result.set_date(date_val);
    } break;
    case AttrType::INTS: {
      float float_val;
      RC    rc = parse_float_prefix(val.value_.pointer_value_, float_val);
      if (rc != RC::SUCCESS) {
        LOG_WARN("Failed to cast char to int, invalid int format. data=%s",
                 static_cast<const char *>(val.value_.pointer_value_));
        return rc;
      }
      result.set_int(static_cast<int>(float_val));
    } break;
    case AttrType::FLOATS: {
      float float_val;
      RC    rc = parse_float_prefix(val.value_.pointer_value_, float_val);
      if (rc != RC::SUCCESS) {
        LOG_WARN("Failed to cast char to float, invalid float format. data=%s",
                 static_cast<const char *>(val.value_.pointer_value_));
        return rc;
      }
      result.set_float(float_val);
    } break;
    case AttrType::VECTORS: {
      float *array  = nullptr;
      int    length = 0;
      RC     rc     = parse_vector_from_string(static_cast<const char *>(val.value_.pointer_value_), array, length);
      if (rc != RC::SUCCESS) {
        LOG_WARN("Failed to cast char to vector, invalid vector format. data=%s",
                 static_cast<const char *>(val.value_.pointer_value_));
        return rc;
      }
      result.set_vector(array, length);
    } break;
    default: return RC::UNSUPPORTED;
  }
  return RC::SUCCESS;
}

int CharType::cast_cost(AttrType type)
{
  if (type == AttrType::CHARS || type == AttrType::TEXTS) {
    return 0;
  }
  if (type == AttrType::DATES) {
    return 1;
  }
  if (type == AttrType::INTS) {
    return 1;
  }
  if (type == AttrType::FLOATS) {
    return 1;
  }
  return INT32_MAX;
}

RC CharType::to_string(const Value &val, string &result) const
{
  stringstream ss;
  ss << val.value_.pointer_value_;
  result = ss.str();
  return RC::SUCCESS;
}