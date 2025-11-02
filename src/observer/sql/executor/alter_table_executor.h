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
// Created by Wangyunlai on 2023/6/13.
//

#pragma once

#include "common/sys/rc.h"
#include "event/session_event.h"

class SQLStageEvent;

/**
 * @brief 更改表的信息
 * @ingroup Executor
 */
class AlterTableExecutor
{
public:
  AlterTableExecutor()          = default;
  virtual ~AlterTableExecutor() = default;

  RC execute(SQLStageEvent *sql_event);
  RC alter_add(SQLStageEvent *sql_event);
  RC alter_drop(SQLStageEvent *sql_event);
  RC alter_change(SQLStageEvent *sql_event);
  RC alter_rename(SQLStageEvent *sql_event);
  RC alter_add_fulltext_index(SQLStageEvent *sql_event);
};