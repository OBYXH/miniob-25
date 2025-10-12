#include "common/type/vector_type.h"
#include "common/log/log.h"
#include <sstream>

int VectorType::compare(const Value &left, const Value &right) const { return INT32_MAX; }

RC VectorType::add(const Value &left, const Value &right, Value &result) const { return RC::UNIMPLEMENTED; }
RC VectorType::subtract(const Value &left, const Value &right, Value &result) const { return RC::UNIMPLEMENTED; }
RC VectorType::multiply(const Value &left, const Value &right, Value &result) const { return RC::UNIMPLEMENTED; }

RC VectorType::set_value_from_str(Value &val, const string &data) const
{
  val.string_to_vector(data.c_str());
  return RC::SUCCESS;
}
RC VectorType::to_string(const Value &val, string &result) const
{
  stringstream ss;
  ss << "[";
  for (int i = 0; i < val.get_vector().size() - 1; i++) {
    ss << val.get_vector()[i] << ",";
  }
  if (val.get_vector().size() > 0) {
    ss << val.get_vector()[val.get_vector().size() - 1];
  }
  ss << "]";
  result = ss.str();
  return RC::SUCCESS;
}