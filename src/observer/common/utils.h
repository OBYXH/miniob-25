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

inline RC parse_float_prefix(const char *str, float &result)
{
  char *end_ptr = nullptr;
  // 清除 errno，以便检查 strtod 是否发生溢出
  errno            = 0;
  double float_val = std::strtod(str, &end_ptr);

  // 检查1: str 和 end_ptr 指向同一位置，说明根本没有解析到数字
  if (str == end_ptr) {
    return RC::INVALID_ARGUMENT;
  }

  // 检查2: 检查解析结束后，后面是否还有非空白的垃圾字符
  while (std::isspace(static_cast<unsigned char>(*end_ptr))) {
    end_ptr++;
  }
  if (*end_ptr != '\0') {
    return RC::INVALID_ARGUMENT;
  }

  // 检查3: 检查是否发生溢出
  if (errno == ERANGE) {
    return RC::INVALID_ARGUMENT;
  }

  result = static_cast<float>(float_val);
  return RC::SUCCESS;
}

inline RC parse_vector_from_string(const char *str, float *&array, int &length)
{
  if (!str || *str != '[') {
    return RC::INVALID_ARGUMENT;
  }

  std::string s = str;
  if (s.back() != ']') {
    return RC::INVALID_ARGUMENT;
  }

  s = s.substr(1, s.size() - 2);  // 去掉开头和结尾的方括号
  std::stringstream ss(s);
  std::string       token;
  size_t            count = 0;

  // 先统计有多少个浮点数
  while (std::getline(ss, token, ',')) {
    count++;
  }

  if (count == 0) {
    return RC::INVALID_ARGUMENT;  // 空数组
  }

  // 分配数组内存
  array  = new float[count];
  length = count * sizeof(float);

  // 重置流并解析浮点数
  ss.clear();
  ss.str(s);
  size_t index = 0;

  while (std::getline(ss, token, ',')) {
    std::stringstream valueStream(token);
    if (!(valueStream >> array[index])) {
      delete[] array;  // 清理已分配内存
      return RC::VECTOR_PARSE_ERROR;
    }
    index++;
  }

  return RC::SUCCESS;
}