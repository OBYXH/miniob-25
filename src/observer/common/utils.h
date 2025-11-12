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
// Created by Longda on 2021/4/14.
//

#pragma once

#include "common/sys/rc.h"
#include <cstdio>
#include <sstream>
#include <cstdlib>

inline bool check_date(int y, int m, int d)
{
  // 定义每个月的天数
  static int mon[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  // 判断是否为闰年
  bool leap = (y % 400 == 0) || (y % 100 != 0 && y % 4 == 0);
  // 检查年份、月份和日期的合法性
  return (y > 0 && y <= 9999) && (m > 0 && m <= 12) &&
         (d > 0 && d <= (mon[m] + (m == 2 && leap ? 1 : 0)));  // 2月如果是闰年则加1天
}

inline RC parse_date(const char *str, int &result)
{
  int y, m, d;
  if (sscanf(str, "%d-%d-%d", &y, &m, &d) != 3) {
    return RC::INVALID_ARGUMENT;
  }
  if (!check_date(y, m, d)) {
    return RC::INVALID_ARGUMENT;
  }
  result = y * 10000 + m * 100 + d;
  return RC::SUCCESS;
}

RC parse_float_prefix(const char *str, float &result)
{
  char *end = nullptr;
  result    = strtof(str, &end);
  return RC::SUCCESS;
}