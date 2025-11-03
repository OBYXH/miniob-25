#pragma once

#include "common/sys/rc.h"
#include "event/sql_event.h"

class SQLStageEvent;

/**
 * @brief 创建视图的执行器
 * @ingroup Executor
 */
class CreateViewExecutor
{
public:
  CreateViewExecutor()          = default;
  virtual ~CreateViewExecutor() = default;

  RC execute(SQLStageEvent *sql_event);
  void init_sys_view_table_attr_infos(std::vector<AttrInfoSqlNode> &attr_infos);
  void make_view_values(std::vector<Value> &values, const std::string &view_name, const std::string &view_definition, bool is_updatable);
};