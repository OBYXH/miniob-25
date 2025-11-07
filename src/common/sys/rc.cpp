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
// Created by Wangyunlai on 2021/5/14.
//

#include "common/sys/rc.h"

const char *strrc(RCCode rc)
{
  switch (rc) {
#define DEFINE_RC(name) \
  case RCCode::name:    \
    return #name;
    DEFINE_RCS
#undef DEFINE_RC
    default: return "UNKNOWN";
  }
}

const char *strrc(RC rc)
{
  return strrc(rc.code());
}

bool OB_SUCC(RCCode rc)
{
  return rc == RCCode::SUCCESS;
}

bool OB_FAIL(RCCode rc)
{
  return rc != RCCode::SUCCESS;
}

bool OB_SUCC(RC rc)
{
  return rc.code() == RCCode::SUCCESS;
}

bool OB_FAIL(RC rc)
{
  return rc.code() != RCCode::SUCCESS;
}
