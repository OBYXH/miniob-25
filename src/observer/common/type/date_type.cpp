#include "common/type/date_type.h"
#include "common/log/log.h"
#include "common/type/attr_type.h"
#include "common/value.h"
#include "common/lang/comparator.h"

int DateType::compare(const Value &left, const Value &right) const
{
  if (right.is_null()) {
    return 1;
  }
  ASSERT(left.attr_type() == AttrType::DATES && right.attr_type() == AttrType::DATES, "invalid cmp type");
  return common::compare_int((void *)&left.value_.int_value_, (void *)&right.value_.int_value_);
}

RC DateType::to_string(const Value &val, string &result) const
{
  int date = val.get_int();

  unsigned int year  = date / 10000;
  unsigned int month = (date / 100) % 100;
  unsigned int day   = date % 100;

  // "YYYY-MM-DD" 需要10个字符，外加一个空终止符
  char buf[11];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
  result = buf;

  return RC::SUCCESS;
}
