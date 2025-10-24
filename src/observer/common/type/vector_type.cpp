#include "common/type/vector_type.h"
#include "common/log/log.h"
#include "common/lang/comparator.h"
#include "common/sys/rc.h"
#include <sstream>
#include <cmath>

int VectorType::compare(const Value &left, const Value &right) const
{
  if (right.is_null()) {
    return 1;
  }
  ASSERT(left.attr_type() == AttrType::VECTORS, "left type is not vector");
  ASSERT(right.attr_type() == AttrType::VECTORS, "right type is not vector");
  ASSERT(left.get_vector().size() == right.get_vector().size(), "vector dimension mismatch, left size: %d, right size: %d",
         left.get_vector().size(),
         right.get_vector().size());
  for (size_t i = 0; i < left.get_vector().size(); i++) {
    auto cmp_result = common::compare_float((void *)&left.get_vector()[i], (void *)&right.get_vector()[i]);
    if (cmp_result != 0) {
      return cmp_result;
    }
  }
  return 0;
}

RC VectorType::add(const Value &left, const Value &right, Value &result) const
{
  ASSERT(left.attr_type() == AttrType::VECTORS, "left type is not vector");
  ASSERT(right.attr_type() == AttrType::VECTORS, "right type is not vector");
  if (left.get_vector().size() != right.get_vector().size()) {
    return RC::VECTOR_DIMENSION_MISMATCH;
  }
  std::vector<float> vec;
  for (size_t i = 0; i < left.get_vector().size(); i++) {
    vec.push_back(round((left.get_vector()[i] + right.get_vector()[i]) * 100) / 100);
  }
  result.set_vector(vec);
  return RC::SUCCESS;
}
RC VectorType::subtract(const Value &left, const Value &right, Value &result) const
{
  ASSERT(left.attr_type() == AttrType::VECTORS, "left type is not vector");
  ASSERT(right.attr_type() == AttrType::VECTORS, "right type is not vector");
  if (left.get_vector().size() != right.get_vector().size()) {
    return RC::VECTOR_DIMENSION_MISMATCH;
  }
  std::vector<float> vec;
  for (size_t i = 0; i < left.get_vector().size(); i++) {
    vec.push_back(round((left.get_vector()[i] - right.get_vector()[i]) * 100) / 100);
  }
  result.set_vector(vec);
  return RC::SUCCESS;
}
RC VectorType::multiply(const Value &left, const Value &right, Value &result) const
{
  ASSERT(left.attr_type() == AttrType::VECTORS, "left type is not vector");
  ASSERT(right.attr_type() == AttrType::VECTORS, "right type is not vector");
  if (left.get_vector().size() != right.get_vector().size()) {
    return RC::VECTOR_DIMENSION_MISMATCH;
  }
  std::vector<float> vec;
  for (size_t i = 0; i < left.get_vector().size(); i++) {
    vec.push_back(round((left.get_vector()[i] * right.get_vector()[i]) * 100) / 100);
  }
  result.set_vector(vec);
  return RC::SUCCESS;
}

RC VectorType::set_value_from_str(Value &val, const string &data) const
{
  val.string_to_vector(data.c_str());
  return RC::SUCCESS;
}
RC VectorType::to_string(const Value &val, string &result) const
{
  stringstream ss;
  ss << "[";
  for (size_t i = 0; i < val.get_vector().size() - 1; i++) {
    ss << val.get_vector()[i] << ",";
  }
  if (val.get_vector().size() > 0) {
    ss << val.get_vector()[val.get_vector().size() - 1];
  }
  ss << "]";
  result = ss.str();
  return RC::SUCCESS;
}