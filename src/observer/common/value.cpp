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
// Created by WangYunlai on 2023/06/28.
//

#include "common/value.h"

#include "common/lang/comparator.h"
#include "common/lang/exception.h"
#include "common/lang/sstream.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/type/attr_type.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include "common/type/date_type.h"
#include <cstdio>
#include <regex>

Value::Value(int val) { set_int(val); }

Value::Value(float val) { set_float(val); }

Value::Value(bool val) { set_boolean(val); }

Value::Value(const char *s, int len /*= 0*/) { set_string(s, len); }

Value *Value::from_date(const char *s)
{
  Value *val = new Value();
  val->set_date(s);
  return val;
}

bool Value::is_valid_date() const
{

  ASSERT(attr_type_ == AttrType::DATES, "attr type is not DATES");
  int date = get_int();

  unsigned int year  = date / 10000;
  unsigned int month = (date / 100) % 100;
  unsigned int day   = date % 100;

  if (year < 1 || year > 9999)  // 简单处理
    return false;
  if (month < 1 || month > 12)
    return false;
  if (day < 1 || day > 31)
    return false;
  if (month == 2) {
    bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (is_leap) {
      if (day > 29)
        return false;
    } else {
      if (day > 28)
        return false;
    }
  } else if (month == 4 || month == 6 || month == 9 || month == 11) {
    if (day > 30)
      return false;
  }

  return true;
}
Value::Value(const string_t &s) { set_string(s.data(), s.size()); }

Value::Value(const Value &other)
{
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->is_null_   = other.is_null_;
  switch (this->attr_type_) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      set_string_from_other(other);
    } break;

    default: {
      this->value_ = other.value_;
    } break;
  }
}

// 所有权转移机制
Value::Value(Value &&other)
{
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->value_     = other.value_;
  this->is_null_   = other.is_null_;
  other.own_data_  = false;
  other.length_    = 0;
}

Value &Value::operator=(const Value &other)
{
  if (this == &other) {
    return *this;
  }
  reset();
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->is_null_   = other.is_null_;
  switch (this->attr_type_) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      set_string_from_other(other);
    } break;

    default: {
      this->value_ = other.value_;
    } break;
  }
  return *this;
}

Value &Value::operator=(Value &&other)
{
  if (this == &other) {
    return *this;
  }
  reset();
  this->attr_type_ = other.attr_type_;
  this->length_    = other.length_;
  this->own_data_  = other.own_data_;
  this->value_     = other.value_;
  this->is_null_   = other.is_null_;
  other.own_data_  = false;
  other.length_    = 0;
  return *this;
}

void Value::reset()
{
  switch (attr_type_) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      if (own_data_ && value_.pointer_value_ != nullptr) {
        delete[] value_.pointer_value_;
        value_.pointer_value_ = nullptr;
      }
    } break;
    default: break;
  }

  attr_type_ = AttrType::UNDEFINED;
  length_    = 0;
  is_null_   = false;
  own_data_  = false;
}

void Value::set_data(char *data, int length)
{
  switch (attr_type_) {
    case AttrType::CHARS: {
      set_string(data, length);
    } break;
    case AttrType::TEXTS: {
      set_text(data, length);
    } break;
    case AttrType::INTS: {
      value_.int_value_ = *(int *)data;
      length_           = length;
    } break;
    case AttrType::FLOATS: {
      value_.float_value_ = *(float *)data;
      length_             = length;
    } break;
    case AttrType::BOOLEANS: {
      value_.bool_value_ = *(int *)data != 0;
      length_            = length;
    } break;
    case AttrType::VECTORS: {
      value_.vector_value_ = (float *)data;
      length_              = length;
      set_vector((float *)data, length);
    } break;
    case AttrType::DATES: {
      value_.int_value_ = *(int *)data;
      length_           = length;
    } break;
    case AttrType::NULLS: {
      is_null_ = true;
      length_  = length;
    } break;
    default: {
      LOG_WARN("unknown data type: %d", attr_type_);
    } break;
  }
}

void Value::set_int(int val)
{
  reset();
  attr_type_        = AttrType::INTS;
  value_.int_value_ = val;
  length_           = sizeof(val);
}

void Value::set_float(float val, int precision /*= 2*/)
{
  reset();
  attr_type_          = AttrType::FLOATS;
  value_.float_value_ = val;
  length_             = sizeof(val);
  float_precision_    = precision;
}
void Value::set_boolean(bool val)
{
  reset();
  attr_type_         = AttrType::BOOLEANS;
  value_.bool_value_ = val;
  length_            = sizeof(val);
}

void Value::set_string(const char *s, int len /*= 0*/)
{
  reset();
  attr_type_ = AttrType::CHARS;
  if (s == nullptr) {
    value_.pointer_value_ = nullptr;
    length_               = 0;
  } else {
    own_data_ = true;
    if (len > 0) {
      len = strnlen(s, len);
    } else {
      len = strlen(s);
    }
    value_.pointer_value_ = new char[len + 1];
    length_               = len;
    memcpy(value_.pointer_value_, s, len);
    value_.pointer_value_[len] = '\0';
  }
}

void Value::set_vector(const std::vector<float> &vec)
{
  attr_type_           = AttrType::VECTORS;
  length_              = vec.size() * sizeof(float);
  value_.vector_value_ = new float[vec.size()];
  for (size_t i = 0; i < vec.size(); i++) {
    value_.vector_value_[i] = vec[i];
  }

  own_data_ = true;
}

void Value::set_vector(float *array, int length)
{
  attr_type_           = AttrType::VECTORS;
  length_              = length;
  value_.vector_value_ = new float[length / sizeof(float)];
  memcpy(value_.vector_value_, array, length);
  own_data_ = true;
}

void Value::set_vector(const char *s)
{
  reset();
  attr_type_                 = AttrType::VECTORS;
  string             vector_ = s;
  std::vector<float> vec_;
  vector_ = vector_.substr(1, vector_.size() - 2);
  std::istringstream iss(vector_);
  string             token;
  while (std::getline(iss, token, ',')) {
    vec_.push_back(stof(token));
  }
  length_              = vec_.size() * sizeof(float);
  value_.vector_value_ = new float[vec_.size()];
  for (size_t i = 0; i < vec_.size(); i++) {
    value_.vector_value_[i] = vec_[i];
  }
}

Value *Value::string_to_vector(const char *s)
{
  Value *val = new Value();
  val->set_vector(s);
  return val;
}

void Value::set_date(const char *s)
{
  reset();
  attr_type_ = AttrType::DATES;

  // 解析日期字符串 YYYY-M[M]-D[D] 格式
  int year = 0, month = 0, day = 0;
  if (sscanf(s, "%d-%d-%d", &year, &month, &day) == 3) {
    // 将日期转换为8位整数格式 YYYYMMDD
    LOG_DEBUG("year: %d, month: %d, day: %d", year, month, day);
    value_.int_value_ = year * 10000 + month * 100 + day;
  } else {
    value_.int_value_ = -1;
  }
  length_ = sizeof(value_.int_value_);
}

void Value::set_date(int val)
{
  reset();
  attr_type_        = AttrType::DATES;
  value_.int_value_ = val;
  length_           = sizeof(val);
}

void Value::set_empty_string(int len)
{
  reset();
  attr_type_ = AttrType::CHARS;

  own_data_             = true;
  value_.pointer_value_ = new char[len + 1];
  length_               = len;
  memset(value_.pointer_value_, 0, len);
  value_.pointer_value_[len] = '\0';
}

void Value::set_text(const char *s, int len /*= 65535*/)
{
  reset();
  attr_type_ = AttrType::TEXTS;
  if (s == nullptr) {
    value_.pointer_value_ = nullptr;
    length_               = 0;
  } else {
    own_data_ = true;
    if (len > 0) {
      len = strnlen(s, len);
    } else {
      len = strlen(s);
    }
    value_.pointer_value_ = new char[len + 1];
    length_               = len;
    memcpy(value_.pointer_value_, s, len);
    value_.pointer_value_[len] = '\0';
  }
}

void Value::set_null(bool is_null /*= true*/)
{
  reset();
  attr_type_ = AttrType::NULLS;
  is_null_   = is_null;
}

void Value::set_value(const Value &value)
{
  switch (value.attr_type_) {
    case AttrType::INTS: {
      set_int(value.get_int());
    } break;
    case AttrType::FLOATS: {
      set_float(value.get_float());
    } break;
    case AttrType::CHARS: {
      set_string(value.get_string().c_str());
    } break;
    case AttrType::BOOLEANS: {
      set_boolean(value.get_boolean());
    } break;
    case AttrType::VECTORS: {
      set_vector(value.get_vector());
    } break;
    case AttrType::DATES: {
      set_date(value.get_int());
    } break;
    case AttrType::TEXTS: {
      set_text(value.get_string().c_str());
    } break;
    case AttrType::NULLS: {
      set_null();
    } break;
    default: {
      ASSERT(false, "got an invalid value type");
    } break;
  }
}

void Value::set_string_from_other(const Value &other)
{
  ASSERT(attr_type_ == AttrType::CHARS || attr_type_ == AttrType::TEXTS, "attr type is not CHARS");
  if (own_data_ && other.value_.pointer_value_ != nullptr && length_ != 0) {
    this->value_.pointer_value_ = new char[this->length_ + 1];
    memcpy(this->value_.pointer_value_, other.value_.pointer_value_, this->length_);
    this->value_.pointer_value_[this->length_] = '\0';
  }
}

char *Value::data() const
{
  switch (attr_type_) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      return value_.pointer_value_;
    } break;
    case AttrType::VECTORS: {
      return (char *)value_.vector_value_;
    } break;
    default: {
      return (char *)&value_;
    } break;
  }
}

string Value::to_string() const
{
  string res;
  RC     rc = DataType::type_instance(this->attr_type_)->to_string(*this, res);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to convert value to string. type=%s", attr_type_to_string(this->attr_type_));
    return "";
  }
  return res;
}

int Value::compare(const Value &other) const
{
  return DataType::type_instance(this->attr_type_)->compare(*this, other);
}

bool Value::LIKE(const Value &other) const
{
  const std::string  left_str  = this->get_string();
  const std::string &right_str = other.get_string();

  std::string regex_str = std::regex_replace(right_str, std::regex("%"), ".*");
  regex_str             = std::regex_replace(regex_str, std::regex("_"), ".");

  std::regex regex_pattern(regex_str);
  return std::regex_match(left_str, regex_pattern);
}

int Value::get_int() const
{
  switch (attr_type_) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      try {
        return (int)(stol(value_.pointer_value_));
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to number. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return 0;
      }
    }
    case AttrType::INTS: {
      return value_.int_value_;
    }
    case AttrType::FLOATS: {
      return (int)(value_.float_value_);
    }
    case AttrType::BOOLEANS: {
      return (int)(value_.bool_value_);
    }
    case AttrType::DATES: {
      return value_.int_value_;
    }
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

float Value::get_float() const
{
  switch (attr_type_) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      try {
        return stof(value_.pointer_value_);
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to float. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return 0.0;
      }
    } break;
    case AttrType::INTS: {
      return float(value_.int_value_);
    } break;
    case AttrType::FLOATS: {
      return value_.float_value_;
    } break;
    case AttrType::BOOLEANS: {
      return float(value_.bool_value_);
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return 0;
    }
  }
  return 0;
}

string Value::get_string() const { return this->to_string(); }

std::vector<float> Value::get_vector() const
{
  assert(attr_type_ == AttrType::VECTORS);
  std::vector<float> vector(get_vector_length());
  for (int i = 0; i < vector.size(); ++i) {
    vector[i] = get_vector_element(i);
  }
  return vector;
}

int Value::get_vector_length() const
{
  assert(attr_type_ == AttrType::VECTORS);
  return length_ / sizeof(float);
}

float Value::get_vector_element(int i) const
{
  assert(attr_type_ == AttrType::VECTORS);
  return value_.vector_value_[i];
}

string_t Value::get_string_t() const
{
  ASSERT(attr_type_ == AttrType::CHARS, "attr type is not CHARS");
  return string_t(value_.pointer_value_, length_);
}

bool Value::get_boolean() const
{
  switch (attr_type_) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      try {
        float val = stof(value_.pointer_value_);
        if (val >= EPSILON || val <= -EPSILON) {
          return true;
        }

        int int_val = stol(value_.pointer_value_);
        if (int_val != 0) {
          return true;
        }

        return value_.pointer_value_ != nullptr;
      } catch (exception const &ex) {
        LOG_TRACE("failed to convert string to float or integer. s=%s, ex=%s", value_.pointer_value_, ex.what());
        return value_.pointer_value_ != nullptr;
      }
    } break;
    case AttrType::INTS: {
      return value_.int_value_ != 0;
    } break;
    case AttrType::FLOATS: {
      float val = value_.float_value_;
      return val >= EPSILON || val <= -EPSILON;
    } break;
    case AttrType::BOOLEANS: {
      return value_.bool_value_;
    } break;
    default: {
      LOG_WARN("unknown data type. type=%d", attr_type_);
      return false;
    }
  }
  return false;
}

RC Value::borrow_text(const Value &v)
{
  ASSERT(v.attr_type_ != AttrType::TEXTS, "attr type is not TEXTS");
  reset();
  this->attr_type_            = AttrType::TEXTS;
  this->length_               = v.length_;
  this->value_.pointer_value_ = v.value_.pointer_value_;
  return RC::SUCCESS;
}
