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
// Created by Wangyunlai on 2022/07/05.
//

#include "sql/expr/expression.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/sys/rc.h"
#include "common/type/attr_type.h"
#include "common/value.h"
#include "sql/expr/tuple.h"
#include "sql/expr/arithmetic_operator.hpp"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include "sql/parser/parse_defs.h"
#include "sql/operator/physical_operator.h"
#include "sql/operator/logical_operator.h"
#include "sql/stmt/select_stmt.h"

class SelectStmt;
class ParsedSqlNode;
class LogicalOperator;
class PhysicalOperator;

using namespace std;

RC VectorToStringExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  RC    rc = RC::SUCCESS;
  Value child_value;
  rc = child_->get_value(tuple, child_value, trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of child expression. rc=%s", strrc(rc));
    return rc;
  }
  if (child_value.attr_type() != AttrType::VECTORS) {
    LOG_WARN("VectorToString function only support vector type");
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  auto str_vec = child_value.to_string();
  value.set_string(str_vec.c_str(), str_vec.size());
  return rc;
}

RC VectorToStringExpr::try_get_value(Value &value) const
{
  RC    rc = RC::SUCCESS;
  Value child_value;
  rc = child_->try_get_value(child_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of child expression. rc=%s", strrc(rc));
    return rc;
  }
  if (child_value.attr_type() != AttrType::VECTORS) {
    LOG_WARN("VectorToString function only support vector type");
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  auto str_vec = child_value.to_string();
  value.set_string(str_vec.c_str(), str_vec.size());
  return rc;
}

// 日期后缀查找函数
std::string get_day_suffix(int day)
{
  if (day >= 11 && day <= 13) {
    return "th";
  }
  switch (day % 10) {
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    default: return "th";
  }
}

// 辅助函数：替换字符串中所有匹配的子串
void replace_all(std::string &subject, const std::string &search, const std::string &replace)
{
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    // 替换操作
    subject.replace(pos, search.length(), replace);
    // 更新位置，从新替换字符串的末尾开始继续查找
    pos += replace.length();
  }
}

void date_format(string format, const Value &child_value, Value &value)
{
  int date = child_value.get_int();

  unsigned int year  = date / 10000;
  unsigned int month = (date / 100) % 100;
  unsigned int day   = date % 100;

  // 数组索引 0 留空（或存储占位符），月份 1 对应索引 1
  const std::vector<std::string> MONTH_NAMES = {"",
      "January",
      "February",
      "March",
      "April",
      "May",
      "June",
      "July",
      "August",
      "September",
      "October",
      "November",
      "December"};
  string                         Y           = to_string(year);
  char                           buf[3];
  snprintf(buf, sizeof(buf), "%02d", year % 100);
  string y = string(buf);
  string M = MONTH_NAMES[month];
  snprintf(buf, sizeof(buf), "%02d", month);
  string m = string(buf);
  string D = to_string(day) + get_day_suffix(day);
  snprintf(buf, sizeof(buf), "%02d", day);
  string                             d          = string(buf);
  std::map<std::string, std::string> format_map = {
      {"%Y", Y},
      {"%y", y},
      {"%M", M},
      {"%m", m},
      {"%D", D},
      {"%d", d},
  };
  string result_format = format;
  for (const auto &pair : format_map) {
    replace_all(result_format, pair.first, pair.second);
  }
  char char_to_remove = '%';
  auto new_end        = std::remove(result_format.begin(), result_format.end(), char_to_remove);
  // 2. 使用 string::erase 移除从逻辑末尾到物理末尾之间的字符。
  result_format.erase(new_end, result_format.end());
  value.set_string(result_format.c_str());
}

RC FunctionExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  RC    rc = RC::SUCCESS;
  Value child_value;
  rc = child_->get_value(tuple, child_value, trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of child expression. rc=%s", strrc(rc));
    return rc;
  }
  switch (function_type_) {
    case Type::LENGTH: {
      if (child_value.is_null()) {
        value.set_null();
        return rc;
      }
      if (child_value.attr_type() != AttrType::CHARS) {
        LOG_WARN("LENGTH function only support string type");
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value.set_int(static_cast<int>(child_value.get_string().size()));
    } break;
    case Type::ROUND: {
      if (child_value.is_null()) {
        value.set_null();
        return rc;
      }
      if (child_value.attr_type() != AttrType::FLOATS) {
        LOG_WARN("ROUND function only support float type");
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value.set_float(child_value.get_float(), precision_);
    } break;
    case Type::DATE_FORMAT: {
      if (child_value.is_null()) {
        value.set_null();
        return rc;
      }
      if (child_value.attr_type() != AttrType::DATES) {
        LOG_WARN("DATE_FORMAT function only support date type");
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      if (common::is_blank(format_.c_str())) {
        value.set_string(format_.c_str());
        return rc;
      }
      date_format(format_, child_value, value);
    } break;
    default: {
      LOG_WARN("unsupported function type: %d", static_cast<int>(function_type_));
      return RC::UNSUPPORTED;
    }
  }
  return rc;
}

RC FunctionExpr::try_get_value(Value &value) const
{
  RC    rc = RC::SUCCESS;
  Value child_value;
  rc = child_->try_get_value(child_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of child expression. rc=%s", strrc(rc));
    return rc;
  }
  switch (function_type_) {
    case Type::LENGTH: {
      if (child_value.is_null()) {
        value.set_null();
        return rc;
      }
      if (child_value.attr_type() != AttrType::CHARS) {
        LOG_WARN("LENGTH function only support string type");
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value.set_int(static_cast<int>(child_value.get_string().size()));
    } break;
    case Type::ROUND: {
      if (child_value.is_null()) {
        value.set_null();
        return rc;
      }
      if (child_value.attr_type() != AttrType::FLOATS) {
        LOG_WARN("ROUND function only support float type");
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value.set_float(child_value.get_float(), precision_);
    } break;
    case Type::DATE_FORMAT: {
      if (child_value.is_null()) {
        value.set_null();
        return rc;
      }
      if (child_value.attr_type() != AttrType::DATES) {
        LOG_WARN("DATE_FORMAT function only support date type");
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      if (common::is_blank(format_.c_str())) {
        value.set_string(format_.c_str());
        return rc;
      }
      date_format(format_, child_value, value);
    } break;
    default: {
      LOG_WARN("unsupported function type: %d", static_cast<int>(function_type_));
      return RC::UNSUPPORTED;
    }
  }
  return rc;
}

RC VecDistanceExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  RC    rc = RC::SUCCESS;
  Value left_value;
  Value right_value;
  rc = left_->get_value(tuple, left_value, trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }
  rc = right_->get_value(tuple, right_value, trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    return rc;
  }
  if (left_value.attr_type() != AttrType::VECTORS || right_value.attr_type() != AttrType::VECTORS) {
    LOG_WARN("vector distance expr only support vector type");
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  if (left_value.get_vector().size() != right_value.get_vector().size()) {
    LOG_WARN("vector dimension mismatch, left size: %d, right size: %d", left_value.get_vector().size(),
             right_value.get_vector().size());
    return RC::VECTOR_DIMENSION_MISMATCH;
  }
  switch (distance_type_) {
    case Type::L2: {
      float sum = 0.0;
      for (size_t i = 0; i < left_value.get_vector().size(); i++) {
        float diff = left_value.get_vector()[i] - right_value.get_vector()[i];
        sum += diff * diff;
      }
      value.set_float(sqrt(sum));
      // value.set_float(round(sqrt(sum) * 100) / 100);
    } break;
    case Type::COSINE: {
      float dot_product = 0.0;
      float left_norm   = 0.0;
      float right_norm  = 0.0;
      for (size_t i = 0; i < left_value.get_vector().size(); i++) {
        dot_product += left_value.get_vector()[i] * right_value.get_vector()[i];
        left_norm += left_value.get_vector()[i] * left_value.get_vector()[i];
        right_norm += right_value.get_vector()[i] * right_value.get_vector()[i];
      }
      if (left_norm == 0 || right_norm == 0) {
        LOG_WARN("vector norm is zero");
        value.set_null();
        return RC::SUCCESS;
      }
      value.set_float(1 - dot_product / (sqrt(left_norm) * sqrt(right_norm)));
      // value.set_float(round((1 - dot_product / (sqrt(left_norm) * sqrt(right_norm))) * 100) / 100);
    } break;
    case Type::INNER: {
      float dot_product = 0.0;
      for (size_t i = 0; i < left_value.get_vector().size(); i++) {
        dot_product += left_value.get_vector()[i] * right_value.get_vector()[i];
      }
      value.set_float(dot_product);
      // value.set_float(round(dot_product * 100) / 100);
    } break;
    default: return RC::UNSUPPORTED;
  }
  return RC::SUCCESS;
}

RC VecDistanceExpr::try_get_value(Value &value) const
{
  RC    rc = RC::SUCCESS;
  Value left_value;
  Value right_value;
  rc = left_->try_get_value(left_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }
  rc = right_->try_get_value(right_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    return rc;
  }
  if (left_value.attr_type() != AttrType::VECTORS || right_value.attr_type() != AttrType::VECTORS) {
    LOG_WARN("vector distance expr only support vector type");
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  if (left_value.get_vector().size() != right_value.get_vector().size()) {
    LOG_WARN("vector dimension mismatch, left size: %d, right size: %d", left_value.get_vector().size(),
             right_value.get_vector().size());
    return RC::VECTOR_DIMENSION_MISMATCH;
  }
  switch (distance_type_) {
    case Type::L2: {
      float sum = 0.0;
      for (size_t i = 0; i < left_value.get_vector().size(); i++) {
        float diff = left_value.get_vector()[i] - right_value.get_vector()[i];
        sum += diff * diff;
      }
      value.set_float(sqrt(sum));
      // value.set_float(round(sqrt(sum) * 100) / 100);
    } break;
    case Type::COSINE: {
      float dot_product = 0.0;
      float left_norm   = 0.0;
      float right_norm  = 0.0;
      for (size_t i = 0; i < left_value.get_vector().size(); i++) {
        dot_product += left_value.get_vector()[i] * right_value.get_vector()[i];
        left_norm += left_value.get_vector()[i] * left_value.get_vector()[i];
        right_norm += right_value.get_vector()[i] * right_value.get_vector()[i];
      }
      if (left_norm == 0 || right_norm == 0) {
        LOG_WARN("vector norm is zero");
        value.set_null();
        return RC::SUCCESS;
      }
      value.set_float(1 - dot_product / (sqrt(left_norm) * sqrt(right_norm)));
      // value.set_float(round((1 - dot_product / (sqrt(left_norm) * sqrt(right_norm))) * 100) / 100);
    } break;
    case Type::INNER: {
      float dot_product = 0.0;
      for (size_t i = 0; i < left_value.get_vector().size(); i++) {
        dot_product += left_value.get_vector()[i] * right_value.get_vector()[i];
      }
      value.set_float(dot_product);
      // value.set_float(round(dot_product * 100) / 100);
    } break;
    default: return RC::UNSUPPORTED;
  }
  return RC::SUCCESS;
}

RC FieldExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  return tuple.find_cell(TupleCellSpec(table_name(), field_name()), value);
}

bool FieldExpr::equal(const Expression &other) const
{
  if (this == &other) {
    return true;
  }
  if (other.type() != ExprType::FIELD) {
    return false;
  }
  const auto &other_field_expr = static_cast<const FieldExpr &>(other);
  return table_name() == other_field_expr.table_name() && field_name() == other_field_expr.field_name();
}

// TODO: 在进行表达式计算时，`chunk` 包含了所有列，因此可以通过 `field_id` 获取到对应列。
// 后续可以优化成在 `FieldExpr` 中存储 `chunk` 中某列的位置信息。
RC FieldExpr::get_column(Chunk &chunk, Column &column)
{
  if (pos_ != -1) {
    column.reference(chunk.column(pos_));
  } else {
    column.reference(chunk.column(field().meta()->field_id()));
  }
  return RC::SUCCESS;
}

bool ValueExpr::equal(const Expression &other) const
{
  if (this == &other) {
    return true;
  }
  if (other.type() != ExprType::VALUE) {
    return false;
  }
  const auto &other_value_expr = static_cast<const ValueExpr &>(other);
  return value_.compare(other_value_expr.get_value()) == 0;
}

RC ValueExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  value = value_;
  return RC::SUCCESS;
}

RC ValueExpr::get_column(Chunk &chunk, Column &column)
{
  column.init(value_, chunk.rows());
  return RC::SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////
CastExpr::CastExpr(unique_ptr<Expression> child, AttrType cast_type) : child_(std::move(child)), cast_type_(cast_type)
{}

CastExpr::~CastExpr() {}

RC CastExpr::cast(const Value &value, Value &cast_value) const
{
  RC rc = RC::SUCCESS;
  if (this->value_type() == value.attr_type()) {
    cast_value = value;
    return rc;
  }
  rc = Value::cast_to(value, cast_type_, cast_value);
  return rc;
}

RC CastExpr::get_value(const Tuple &tuple, Value &result, Trx *trx) const
{
  Value value;
  RC    rc = child_->get_value(tuple, value, trx);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  return cast(value, result);
}

RC CastExpr::get_column(Chunk &chunk, Column &column)
{
  Column child_column;
  RC     rc = child_->get_column(chunk, child_column);
  if (rc != RC::SUCCESS) {
    return rc;
  }
  column.init(cast_type_, child_column.attr_len());
  for (int i = 0; i < child_column.count(); ++i) {
    Value value = child_column.get_value(i);
    Value cast_value;
    rc = cast(value, cast_value);
    if (rc != RC::SUCCESS) {
      return rc;
    }
    column.append_value(cast_value);
  }
  return rc;
}

RC CastExpr::try_get_value(Value &result) const
{
  Value value;
  RC    rc = child_->try_get_value(value);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  return cast(value, result);
}

////////////////////////////////////////////////////////////////////////////////

ComparisonExpr::ComparisonExpr(CompOp comp, unique_ptr<Expression> left, unique_ptr<Expression> right)
    : comp_(comp), left_(std::move(left)), right_(std::move(right))
{}

ComparisonExpr::~ComparisonExpr() {}

RC ComparisonExpr::compare_value(const Value &left, const Value &right, bool &result) const
{
  if (left.is_null() || right.is_null()) {
    if (comp_ == IS_OP) {
      result = left.is_null() && right.is_null();
    } else if (comp_ == IS_NOT_OP) {
      result = !(left.is_null() && right.is_null());
    } else {
      result = false;
    }
    return RC::SUCCESS;
  }
  RC rc = RC::SUCCESS;
  if (comp_ == LIKE_OP || comp_ == NOT_LIKE_OP) {
    ASSERT(left.is_str() && right.is_str(), "LIKE ONLY SUPPORT STRING TYPE!");
    result = comp_ == LIKE_OP ? left.LIKE(right) : !left.LIKE(right);
    return rc;
  }

  int cmp_result = left.compare(right);
  result         = false;
  switch (comp_) {
    case IN_OP:
    case EQUAL_TO: {
      result = (0 == cmp_result);
    } break;
    case LESS_EQUAL: {
      result = (cmp_result <= 0);
    } break;
    case NOT_IN_OP:
    case NOT_EQUAL: {
      result = (cmp_result != 0);
    } break;
    case LESS_THAN: {
      result = (cmp_result < 0);
    } break;
    case GREAT_EQUAL: {
      result = (cmp_result >= 0);
    } break;
    case GREAT_THAN: {
      result = (cmp_result > 0);
    } break;
    default: {
      LOG_WARN("unsupported comparison. %d", comp_);
      rc = RC::INTERNAL;
    } break;
  }

  return rc;
}

RC ComparisonExpr::try_get_value(Value &cell) const
{
  if (left_->type() == ExprType::VALUE && right_->type() == ExprType::VALUE) {
    ValueExpr   *left_value_expr  = static_cast<ValueExpr *>(left_.get());
    ValueExpr   *right_value_expr = static_cast<ValueExpr *>(right_.get());
    const Value &left_cell        = left_value_expr->get_value();
    const Value &right_cell       = right_value_expr->get_value();

    bool value = false;
    RC   rc    = compare_value(left_cell, right_cell, value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to compare tuple cells. rc=%s", strrc(rc));
    } else {
      cell.set_boolean(value);
    }
    return rc;
  }

  return RC::INVALID_ARGUMENT;
}

RC ComparisonExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  Value left_value;
  Value right_value;
  RC    rc = RC::SUCCESS;

  // 子查询处理（排列组合共三类）
  if (left_->type() == ExprType::SUBQUERY && right_->type() == ExprType::SUBQUERY) {
    rc = left_->get_value(tuple, left_value, trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
      return rc;
    }
    // 对于这种情况子查询只支持一行
    Value _test;
    if (left_->get_value(tuple, _test, trx) != RC::RECORD_EOF) {
      LOG_WARN("we only support 1 rows for subquery result rc=%s", strrc(rc));
      return RC::INTERNAL;
    }
    rc = right_->get_value(tuple, right_value, trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }
    if (right_->get_value(tuple, _test, trx) != RC::RECORD_EOF) {
      LOG_WARN("we only support 1 rows for subquery result rc=%s", strrc(rc));
      return RC::INTERNAL;
    }
    bool bool_value = false;
    rc              = compare_value(left_value, right_value, bool_value);
    if (rc == RC::SUCCESS) {
      value.set_boolean(bool_value);
    }
  } else if (left_->type() == ExprType::SUBQUERY || right_->type() == ExprType::SUBQUERY) {
    SubqueryExpr *subquery_expr;
    Value        *sub_query_value;
    if (left_->type() == ExprType::SUBQUERY) {
      subquery_expr = static_cast<SubqueryExpr *>(left_.get());
      if (right_->type() != ExprType::SPECIAL) {
        rc = right_->get_value(tuple, right_value, trx);
      }
      sub_query_value = &left_value;
    } else {
      subquery_expr = static_cast<SubqueryExpr *>(right_.get());
      if (left_->type() != ExprType::SPECIAL) {
        rc = left_->get_value(tuple, left_value, trx);
      }
      sub_query_value = &right_value;
    }
    if (rc != RC::SUCCESS) {
      LOG_WARN("ComparisonExpr:Subquery: failed to get value of expression. rc=%s", strrc(rc));
      return rc;
    }

    bool bool_value = false;
    // 循环执行子查询的算子，直到找到一个满足条件的值
    bool has_sub_queried_ = false;
    while ((rc = subquery_expr->get_value(tuple, *sub_query_value, trx)) == RC::SUCCESS) {

      if (comp_ == EXISTS_OP) {
        bool_value = true;
        value.set_boolean(true);
        break;
      } else if (comp_ == NOT_EXISTS_OP) {
        bool_value = false;
        value.set_boolean(false);
        break;
        // 当 comp_ 不是 IN、NOT_IN、EXISTS、NOT_EXISTS 时，子查询的结果只能是一个值
      } else if (comp_ != IN_OP && comp_ != NOT_IN_OP && comp_ != EXISTS_OP && comp_ != NOT_EXISTS_OP) {
        if (has_sub_queried_) {
          has_sub_queried_ = false;
          rc               = RC::SUB_QUERY_VALUES_DISMATCH;
          break;
        } else {
          has_sub_queried_ = true;
        }
      }

      // CompOp == EXISTS/NOT_EXISTS 不可能走到这里

      rc = compare_value(left_value, right_value, bool_value);

      // 对 IN/NOT_IN 的 fast-break 逻辑
      // 当 CompOp 是 IN 时，只要找到一个满足条件的值就可以返回
      // 当 CompOp 是 NOT_IN 时，必须得遍历完所有的值才能知道
      if (rc == RC::SUCCESS && comp_ == IN_OP && bool_value) {
        value.set_boolean(bool_value);
        break;
      } else if (rc == RC::SUCCESS && comp_ == NOT_IN_OP && !bool_value) {
        // 当 CompOp==NOT_IN，且左右 value 相等（因为是NOTIN，所以bool_value前要加!），直接结束循环
        value.set_boolean(bool_value);
        break;
      }
    }

    if (rc == RC::INVALID_ARGUMENT || rc == RC::SUB_QUERY_VALUES_DISMATCH) {
      if (subquery_expr->close_physical_operator() != RC::SUCCESS) {
        LOG_WARN("failed to close physical operator.");
      }
      return rc;
    }

    // 执行到了算子末尾，还没有找到满足条件的值
    if (rc == RC::RECORD_EOF) {
      if (comp_ == NOT_IN_OP || comp_ == NOT_EXISTS_OP) {
        bool_value = true;
        value.set_boolean(true);
        rc = RC::SUCCESS;
        return rc;
      } else if (comp_ == IN_OP || comp_ == EXISTS_OP) {
        bool_value = false;
        rc         = RC::SUCCESS;
        return rc;
      }
    }
    if (rc != RC::SUCCESS && rc != RC::RECORD_EOF) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    }
    value.set_boolean(bool_value);

    // 关闭算子
    // 可优化 static_cast 潜在的开销
    if (subquery_expr->physical_operator() != nullptr) {
      rc = subquery_expr->close_physical_operator();
      if (rc != RC::SUCCESS) {
        LOG_WARN("failed to close physical operator. rc=%s", strrc(rc));
      }
    }

  } else if (left_->type() == ExprType::VALUES || right_->type() == ExprType::VALUES) {
    ValueListExpr *value_list_expr;
    Value         *value_list_value;
    if (left_->type() == ExprType::VALUES) {
      value_list_expr  = static_cast<ValueListExpr *>(left_.get());
      rc               = right_->get_value(tuple, right_value, trx);  // 假设右边不是value list
      value_list_value = &left_value;
    } else {
      value_list_expr = static_cast<ValueListExpr *>(right_.get());
      if (left_->type() != ExprType::SPECIAL) {
        rc = left_->get_value(tuple, left_value, trx);  // 假设左边不是value list
      }
      value_list_value = &right_value;
    }
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }

    bool bool_value       = false;
    bool has_sub_queried_ = false;

    // 循环执行子查询的算子，直到找到一个满足条件的值
    while ((rc = value_list_expr->get_value(tuple, *value_list_value, trx)) == RC::SUCCESS) {
      if (value_list_value->attr_type() == AttrType::UNDEFINED) {
        rc = RC::RECORD_EOF;  // maybe wrong
        break;
      }

      if (comp_ == EXISTS_OP) {
        // 当comp_为EXISTS时，直接返回true
        bool_value = true;
        value.set_boolean(true);
        break;
      } else if (comp_ == NOT_EXISTS_OP) {
        bool_value = false;
        value.set_boolean(false);
        break;
      } else if (comp_ == CompOp::EQUAL_TO || comp_ == CompOp::NOT_EQUAL) {
        if (has_sub_queried_) {
          has_sub_queried_ = false;
          rc               = RC::INVALID_ARGUMENT;
          break;
        } else {
          has_sub_queried_ = true;
        }
      }

      rc = compare_value(left_value, right_value, bool_value);

      if (rc == RC::SUCCESS && comp_ != NOT_IN_OP && bool_value) {
        value.set_boolean(bool_value);
        break;
      } else if (rc == RC::SUCCESS && comp_ == NOT_IN_OP && !bool_value) {
        value.set_boolean(bool_value);
        break;
      }
    }

    // if (rc == RC::INVALID_ARGUMENT)
    //   return rc;

    // EOF判断
    if (rc == RC::RECORD_EOF) {
      if (comp_ == NOT_IN_OP || comp_ == NOT_EXISTS_OP) {
        value.set_boolean(true);
        rc = RC::SUCCESS;
        return rc;
      } else if (comp_ == IN_OP || comp_ == EXISTS_OP) {
        value.set_boolean(false);
        rc = RC::SUCCESS;
        return rc;
      }
    }
    if (rc != RC::SUCCESS && rc != RC::RECORD_EOF) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    }

    value.set_boolean(bool_value);

    value_list_expr->set_index(0);  // 重置index
  } else {                          // 普通表达式
    // exists 和 not exists 不应该走到这里，TA们是用于子查询的。
    if (comp_ == EXISTS_OP || comp_ == NOT_EXISTS_OP) {
      LOG_WARN("exists and not exists should be used in subquery");
      return RC::INVALID_ARGUMENT;
    }

    rc = left_->get_value(tuple, left_value, trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
      return rc;
    }
    rc = right_->get_value(tuple, right_value, trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }

    bool bool_value = false;

    rc = compare_value(left_value, right_value, bool_value);
    if (rc == RC::SUCCESS) {
      value.set_boolean(bool_value);
    }
  }

  if (rc == RC::RECORD_EOF)
    rc = RC::SUCCESS;

  return rc;
}

RC ComparisonExpr::eval(Chunk &chunk, vector<uint8_t> &select)
{
  RC     rc = RC::SUCCESS;
  Column left_column;
  Column right_column;

  rc = left_->get_column(chunk, left_column);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }
  rc = right_->get_column(chunk, right_column);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    return rc;
  }
  if (left_column.attr_type() != right_column.attr_type()) {
    LOG_WARN("cannot compare columns with different types");
    return RC::INTERNAL;
  }
  if (left_column.attr_type() == AttrType::INTS) {
    rc = compare_column<int>(left_column, right_column, select);
  } else if (left_column.attr_type() == AttrType::FLOATS) {
    rc = compare_column<float>(left_column, right_column, select);
  } else if (left_column.attr_type() == AttrType::CHARS) {
    int rows = 0;
    if (left_column.column_type() == Column::Type::CONSTANT_COLUMN) {
      rows = right_column.count();
    } else {
      rows = left_column.count();
    }
    for (int i = 0; i < rows; ++i) {
      Value left_val  = left_column.get_value(i);
      Value right_val = right_column.get_value(i);
      bool  result    = false;
      rc              = compare_value(left_val, right_val, result);
      if (rc != RC::SUCCESS) {
        LOG_WARN("failed to compare tuple cells. rc=%s", strrc(rc));
        return rc;
      }
      select[i] &= result ? 1 : 0;
    }

  } else {
    LOG_WARN("unsupported data type %d", left_column.attr_type());
    return RC::INTERNAL;
  }
  return rc;
}

template <typename T>
RC ComparisonExpr::compare_column(const Column &left, const Column &right, vector<uint8_t> &result) const
{
  RC rc = RC::SUCCESS;

  bool left_const  = left.column_type() == Column::Type::CONSTANT_COLUMN;
  bool right_const = right.column_type() == Column::Type::CONSTANT_COLUMN;
  if (left_const && right_const) {
    compare_result<T, true, true>((T *)left.data(), (T *)right.data(), left.count(), result, comp_);
  } else if (left_const && !right_const) {
    compare_result<T, true, false>((T *)left.data(), (T *)right.data(), right.count(), result, comp_);
  } else if (!left_const && right_const) {
    compare_result<T, false, true>((T *)left.data(), (T *)right.data(), left.count(), result, comp_);
  } else {
    compare_result<T, false, false>((T *)left.data(), (T *)right.data(), left.count(), result, comp_);
  }
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
ConjunctionExpr::ConjunctionExpr(Type type, vector<unique_ptr<Expression>> &children)
    : conjunction_type_(type), children_(std::move(children))
{}

RC ConjunctionExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  RC rc = RC::SUCCESS;
  if (children_.empty()) {
    value.set_boolean(true);
    return rc;
  }

  Value tmp_value;
  for (const unique_ptr<Expression> &expr : children_) {
    rc = expr->get_value(tuple, tmp_value, trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value by child expression. rc=%s", strrc(rc));
      return rc;
    }
    bool bool_value = tmp_value.get_boolean();
    if ((conjunction_type_ == Type::AND && !bool_value) || (conjunction_type_ == Type::OR && bool_value)) {
      value.set_boolean(bool_value);
      return rc;
    }
  }

  bool default_value = (conjunction_type_ == Type::AND);
  value.set_boolean(default_value);
  return rc;
}

////////////////////////////////////////////////////////////////////////////////

ArithmeticExpr::ArithmeticExpr(ArithmeticExpr::Type type, Expression *left, Expression *right)
    : arithmetic_type_(type), left_(left), right_(right)
{}
ArithmeticExpr::ArithmeticExpr(ArithmeticExpr::Type type, unique_ptr<Expression> left, unique_ptr<Expression> right)
    : arithmetic_type_(type), left_(std::move(left)), right_(std::move(right))
{}

bool ArithmeticExpr::equal(const Expression &other) const
{
  if (this == &other) {
    return true;
  }
  if (type() != other.type()) {
    return false;
  }
  auto &other_arith_expr = static_cast<const ArithmeticExpr &>(other);
  return arithmetic_type_ == other_arith_expr.arithmetic_type() && left_->equal(*other_arith_expr.left_) &&
         right_->equal(*other_arith_expr.right_);
}
AttrType ArithmeticExpr::value_type() const
{
  if (!right_) {
    return left_->value_type();
  }
  if (!left_) {
    return right_->value_type();
  }

  if ((left_->value_type() == AttrType::INTS) && (right_->value_type() == AttrType::INTS) &&
      arithmetic_type_ != Type::DIV) {
    return AttrType::INTS;
  } else if (left_->value_type() == AttrType::FLOATS && right_->value_type() == AttrType::FLOATS) {
    return AttrType::FLOATS;
  } else if (left_->value_type() == AttrType::VECTORS && right_->value_type() == AttrType::VECTORS) {
    return AttrType::VECTORS;
  } else {
    return AttrType::FLOATS;
  }
}

RC ArithmeticExpr::calc_value(const Value &left_value, const Value &right_value, Value &value) const
{
  RC rc = RC::SUCCESS;

  const AttrType target_type = value_type();
  value.set_type(target_type);
  if (left_value.is_null() || right_value.is_null()) {
    value.set_null();
    return RC::SUCCESS;
  }

  switch (arithmetic_type_) {
    case Type::ADD: {
      rc = Value::add(left_value, right_value, value);
    } break;

    case Type::SUB: {
      rc = Value::subtract(left_value, right_value, value);
    } break;

    case Type::MUL: {
      rc = Value::multiply(left_value, right_value, value);
    } break;

    case Type::DIV: {
      rc = Value::divide(left_value, right_value, value);
    } break;

    case Type::NEGATIVE: {
      rc = Value::negative(right_value, value);
    } break;

    default: {
      rc = RC::INTERNAL;
      LOG_WARN("unsupported arithmetic type. %d", arithmetic_type_);
    } break;
  }
  return rc;
}

template <bool LEFT_CONSTANT, bool RIGHT_CONSTANT>
RC ArithmeticExpr::execute_calc(
    const Column &left, const Column &right, Column &result, Type type, AttrType attr_type) const
{
  RC rc = RC::SUCCESS;
  switch (type) {
    case Type::ADD: {
      if (attr_type == AttrType::INTS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, int, AddOperator>(
            (int *)left.data(), (int *)right.data(), (int *)result.data(), result.capacity());
      } else if (attr_type == AttrType::FLOATS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, float, AddOperator>(
            (float *)left.data(), (float *)right.data(), (float *)result.data(), result.capacity());
      } else {
        rc = RC::UNIMPLEMENTED;
      }
    } break;
    case Type::SUB:
      if (attr_type == AttrType::INTS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, int, SubtractOperator>(
            (int *)left.data(), (int *)right.data(), (int *)result.data(), result.capacity());
      } else if (attr_type == AttrType::FLOATS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, float, SubtractOperator>(
            (float *)left.data(), (float *)right.data(), (float *)result.data(), result.capacity());
      } else {
        rc = RC::UNIMPLEMENTED;
      }
      break;
    case Type::MUL:
      if (attr_type == AttrType::INTS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, int, MultiplyOperator>(
            (int *)left.data(), (int *)right.data(), (int *)result.data(), result.capacity());
      } else if (attr_type == AttrType::FLOATS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, float, MultiplyOperator>(
            (float *)left.data(), (float *)right.data(), (float *)result.data(), result.capacity());
      } else {
        rc = RC::UNIMPLEMENTED;
      }
      break;
    case Type::DIV:
      if (attr_type == AttrType::INTS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, int, DivideOperator>(
            (int *)left.data(), (int *)right.data(), (int *)result.data(), result.capacity());
      } else if (attr_type == AttrType::FLOATS) {
        binary_operator<LEFT_CONSTANT, RIGHT_CONSTANT, float, DivideOperator>(
            (float *)left.data(), (float *)right.data(), (float *)result.data(), result.capacity());
      } else {
        rc = RC::UNIMPLEMENTED;
      }
      break;
    case Type::NEGATIVE:
      if (attr_type == AttrType::INTS) {
        unary_operator<LEFT_CONSTANT, int, NegateOperator>((int *)left.data(), (int *)result.data(), result.capacity());
      } else if (attr_type == AttrType::FLOATS) {
        unary_operator<LEFT_CONSTANT, float, NegateOperator>(
            (float *)left.data(), (float *)result.data(), result.capacity());
      } else {
        rc = RC::UNIMPLEMENTED;
      }
      break;
    default: rc = RC::UNIMPLEMENTED; break;
  }
  if (rc == RC::SUCCESS) {
    result.set_count(result.capacity());
  }
  return rc;
}

RC ArithmeticExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  RC rc = RC::SUCCESS;

  Value left_value;
  Value right_value;

  if (left_) {
    rc = left_->get_value(tuple, left_value, trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
      return rc;
    }
  }
  if (right_) {
    rc = right_->get_value(tuple, right_value, trx);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }
  }

  return calc_value(left_value, right_value, value);
}

RC ArithmeticExpr::get_column(Chunk &chunk, Column &column)
{
  RC rc = RC::SUCCESS;
  if (pos_ != -1) {
    column.reference(chunk.column(pos_));
    return rc;
  }
  Column left_column;
  Column right_column;

  rc = left_->get_column(chunk, left_column);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get column of left expression. rc=%s", strrc(rc));
    return rc;
  }
  rc = right_->get_column(chunk, right_column);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get column of right expression. rc=%s", strrc(rc));
    return rc;
  }
  return calc_column(left_column, right_column, column);
}

RC ArithmeticExpr::calc_column(const Column &left_column, const Column &right_column, Column &column) const
{
  RC rc = RC::SUCCESS;

  const AttrType target_type = value_type();
  column.init(target_type, left_column.attr_len(), max(left_column.count(), right_column.count()));
  bool left_const  = left_column.column_type() == Column::Type::CONSTANT_COLUMN;
  bool right_const = right_column.column_type() == Column::Type::CONSTANT_COLUMN;
  if (left_const && right_const) {
    column.set_column_type(Column::Type::CONSTANT_COLUMN);
    rc = execute_calc<true, true>(left_column, right_column, column, arithmetic_type_, target_type);
  } else if (left_const && !right_const) {
    column.set_column_type(Column::Type::NORMAL_COLUMN);
    rc = execute_calc<true, false>(left_column, right_column, column, arithmetic_type_, target_type);
  } else if (!left_const && right_const) {
    column.set_column_type(Column::Type::NORMAL_COLUMN);
    rc = execute_calc<false, true>(left_column, right_column, column, arithmetic_type_, target_type);
  } else {
    column.set_column_type(Column::Type::NORMAL_COLUMN);
    rc = execute_calc<false, false>(left_column, right_column, column, arithmetic_type_, target_type);
  }
  return rc;
}

RC ArithmeticExpr::try_get_value(Value &value) const
{
  RC rc = RC::SUCCESS;

  Value left_value;
  Value right_value;

  // 防止空指针问题
  if (left_) {
    rc = left_->try_get_value(left_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
      return rc;
    }
  }

  if (right_) {
    rc = right_->try_get_value(right_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }
  }

  return calc_value(left_value, right_value, value);
}

////////////////////////////////////////////////////////////////////////////////

UnboundAggregateExpr::UnboundAggregateExpr(const char *aggregate_name, Expression *child)
    : aggregate_name_(aggregate_name), child_(child)
{}

UnboundAggregateExpr::UnboundAggregateExpr(const char *aggregate_name, unique_ptr<Expression> child)
    : aggregate_name_(aggregate_name), child_(std::move(child))
{}

////////////////////////////////////////////////////////////////////////////////
AggregateExpr::AggregateExpr(Type type, Expression *child) : aggregate_type_(type), child_(child) {}

AggregateExpr::AggregateExpr(Type type, unique_ptr<Expression> child) : aggregate_type_(type), child_(std::move(child))
{}

RC AggregateExpr::get_column(Chunk &chunk, Column &column)
{
  RC rc = RC::SUCCESS;
  if (pos_ != -1) {
    column.reference(chunk.column(pos_));
  } else {
    rc = RC::INTERNAL;
  }
  return rc;
}

bool AggregateExpr::equal(const Expression &other) const
{
  if (this == &other) {
    return true;
  }
  if (other.type() != type()) {
    return false;
  }
  const AggregateExpr &other_aggr_expr = static_cast<const AggregateExpr &>(other);
  return aggregate_type_ == other_aggr_expr.aggregate_type() && child_->equal(*other_aggr_expr.child());
}

unique_ptr<Aggregator> AggregateExpr::create_aggregator() const
{
  unique_ptr<Aggregator> aggregator;
  switch (aggregate_type_) {
    case Type::SUM: {
      aggregator = make_unique<SumAggregator>();
      break;
    }
    case Type::COUNT: {
      aggregator = make_unique<CountAggregator>();
      break;
    }
    case Type::AVG: {
      aggregator = make_unique<AvgAggregator>();
      break;
    }
    case Type::MAX: {
      aggregator = make_unique<MaxAggregator>();
      break;
    }
    case Type::MIN: {
      aggregator = make_unique<MinAggregator>();
      break;
    }
    default: {
      ASSERT(false, "unsupported aggregate type");
      break;
    }
  }
  return aggregator;
}

RC AggregateExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  return tuple.find_cell(TupleCellSpec(name()), value);
}

RC AggregateExpr::type_from_string(const char *type_str, AggregateExpr::Type &type)
{
  RC rc = RC::SUCCESS;
  if (0 == strcasecmp(type_str, "count")) {
    type = Type::COUNT;
  } else if (0 == strcasecmp(type_str, "sum")) {
    type = Type::SUM;
  } else if (0 == strcasecmp(type_str, "avg")) {
    type = Type::AVG;
  } else if (0 == strcasecmp(type_str, "max")) {
    type = Type::MAX;
  } else if (0 == strcasecmp(type_str, "min")) {
    type = Type::MIN;
  } else {
    rc = RC::INVALID_ARGUMENT;
  }
  return rc;
}

////////////////////////////////////////////////////////////////////////////////

SubqueryExpr::SubqueryExpr(ParsedSqlNode *sub_query_sn) : sub_query_sn_(sub_query_sn) {}

RC SubqueryExpr::open_physical_operator(Tuple *outer_tuple) const
{
  if (physical_operator_ == nullptr) {
    LOG_WARN("physical operator is null");
    return RC::INVALID_ARGUMENT;
  }
  // 将外层的 tuple 传递给子查询算子，以达到查外层表的目的
  // proj -> orderby -> predicate 普通
  // proj -> orderby -> groupby -> predicate 聚合
  physical_operator_->set_outer_tuple(outer_tuple);
  RC rc = physical_operator_->open(trx_);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open physical operator. rc=%s", strrc(rc));
  } else {
    is_open_ = true;
  }
  return rc;
}
RC SubqueryExpr::close_physical_operator() const
{
  if (physical_operator_ == nullptr) {
    LOG_WARN("physical operator is null");
    return RC::INVALID_ARGUMENT;
  }
  RC rc = physical_operator_->close();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to close physical operator. rc=%s", strrc(rc));
  } else {
    is_open_ = false;
  }
  return rc;
}

AttrType SubqueryExpr::value_type() const { return AttrType::INTS; }
int      SubqueryExpr::value_length() const { return sizeof(int); }
RC       SubqueryExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  RC rc = RC::SUCCESS;
  if (logical_operator_ == nullptr && physical_operator_ == nullptr) {
    return RC::RECORD_EOF;
  }

  if (physical_operator_ == nullptr) {
    LOG_WARN("physical operator is null");
    return RC::INVALID_ARGUMENT;
  }

  trx_ = trx;

  auto *tuple__ = const_cast<Tuple*>(&tuple);
  if (!is_open_) {
    rc = open_physical_operator(tuple__);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to open physical operator. rc=%s", strrc(rc));
      return rc;
    }
  }

  // 开始执行物理操作
  rc = physical_operator_->next();
  if (rc != RC::SUCCESS) {
    // 可能 EOF 了
    if (rc != RC::RECORD_EOF) {
      close_physical_operator();  // 关闭子查询算子
      LOG_PANIC("failed to get next tuple. rc=%s", strrc(rc));
      return rc;
    }
    rc = close_physical_operator();
    if (rc == RC::SUCCESS) {
      rc = RC::RECORD_EOF;
    } else {
      LOG_PANIC("failed to close physical operator. rc=%s", strrc(rc));
    }
    return rc;
  }
  auto tuple_ = physical_operator_->current_tuple();
  if (tuple_->cell_num() > 1) {
    LOG_WARN("tuple cell count is not 1");
    close_physical_operator();  // 关闭子查询算子
    return RC::INVALID_ARGUMENT;
  }

  if (tuple_->cell_num() == 0) {
    LOG_WARN("A warn from SubqueryExpr: tuple cell count is 0");
  }
  tuple_->cell_at(0, value);
  return rc;
}

void SubqueryExpr::set_logical_operator(std::unique_ptr<LogicalOperator> logical_operator)
{
  logical_operator_ = std::move(logical_operator);
}
void SubqueryExpr::set_physical_operator(std::unique_ptr<PhysicalOperator> physical_operator)
{
  physical_operator_ = std::move(physical_operator);
}
void                               SubqueryExpr::set_trx(Trx *trx) { trx_ = trx; }
void                               SubqueryExpr::set_stmt(std::unique_ptr<SelectStmt> stmt) { stmt_ = std::move(stmt); }
ParsedSqlNode                     *SubqueryExpr::sub_query_sn() { return sub_query_sn_; }
std::unique_ptr<SelectStmt>       &SubqueryExpr::stmt() { return stmt_; }
std::unique_ptr<LogicalOperator>  &SubqueryExpr::logical_operator() { return logical_operator_; }
std::unique_ptr<PhysicalOperator> &SubqueryExpr::physical_operator() { return physical_operator_; }

RC ValueListExpr::get_value(const Tuple &tuple, Value &value, Trx *trx) const
{
  if (index_ >= values_.size()) {
    index_ = 0;
    return RC::RECORD_EOF;
  }
  value = values_[index_++];
  return RC::SUCCESS;
}