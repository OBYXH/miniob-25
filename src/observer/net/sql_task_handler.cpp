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
// Created by Wangyunlai on 2024/01/10.
//

#include "net/sql_task_handler.h"
#include "net/communicator.h"
#include "event/session_event.h"
#include "event/sql_event.h"
#include "session/session.h"
#include "sql/parser/parse_defs.h"
#include "storage/db/db.h"

RC SqlTaskHandler::handle_event(Communicator *communicator)
{
  SessionEvent *event = nullptr;
  RC            rc    = communicator->read_event(event);
  if (OB_FAIL(rc)) {
    return rc;
  }

  if (nullptr == event) {
    return RC::SUCCESS;
  }

  session_stage_.handle_request2(event);

  SQLStageEvent sql_event(event, event->query());

  rc = handle_sql(&sql_event);
  if (OB_FAIL(rc)) {
    LOG_TRACE("failed to handle sql. rc=%s", strrc(rc));
    event->sql_result()->set_return_code(rc);
  }

  bool need_disconnect = false;

  rc = communicator->write_result(event, need_disconnect);
  LOG_INFO("write result return %s", strrc(rc));
  event->session()->set_current_request(nullptr);
  Session::set_current_session(nullptr);

  delete event;

  if (need_disconnect) {
    return RC_WITH_LOCATION(RC::INTERNAL, "");
  }
  return RC::SUCCESS;
}

RC SqlTaskHandler::handle_sql(SQLStageEvent *sql_event)
{
  RC rc = query_cache_stage_.handle_request(sql_event);
  if (OB_FAIL(rc)) {
    LOG_TRACE("failed to do query cache. rc=%s", strrc(rc));
    return rc;
  }

  rc = parse_stage_.handle_request(sql_event);
  if (OB_FAIL(rc)) {
    LOG_TRACE("failed to do parse. rc=%s", strrc(rc));
    return rc;
  }

  // 递归展开嵌套视图
  if (sql_event->sql_node()->flag == SCF_SELECT || 
      sql_event->sql_node()->flag == SCF_INSERT ||
      sql_event->sql_node()->flag == SCF_UPDATE ||
      sql_event->sql_node()->flag == SCF_DELETE) {
    
    auto *db = sql_event->session_event()->session()->get_current_db();
    if (db == nullptr) return RC_WITH_LOCATION(RC::INTERNAL, "");

    // 递归展开视图，直到没有新的视图需要展开
    std::set<std::string> expanded_views; // 防止循环引用
    while (true) {
      std::vector<std::string> view_names;
      const ParsedSqlNode *current_sql_node = nullptr;
      const auto *last_view_node = sql_event->get_last_sql_node_view();
      if (last_view_node != nullptr) {
        current_sql_node = last_view_node->get();
      } else {
        current_sql_node = sql_event->sql_node().get();
      }      
      // 收集当前SQL中的所有表/视图名
      switch (current_sql_node->flag) {
        case SCF_SELECT:
          for (auto &relation : current_sql_node->selection.relations) {
            view_names.push_back(relation.relation_name);
          }
          break;
        case SCF_INSERT:
          view_names.push_back(sql_event->sql_node()->insertion.relation_name);
          break;
        case SCF_UPDATE:
          view_names.push_back(sql_event->sql_node()->update.relation_name);
          break;
        case SCF_DELETE:
          view_names.push_back(sql_event->sql_node()->deletion.relation_name);
          break;
        default:
          break;
      }

      // 检查是否有新的视图需要展开
      bool has_new_views = false;
      for (auto &view_name : view_names) {
        if (expanded_views.count(view_name) > 0) {
          continue; // 已经展开过
        }
        
        View *view = db->find_view(view_name.c_str());
        if (view == nullptr) {
          continue; // 不是视图
        }
        
        // 展开视图
        sql_event->add_view_sql(view->view_definition());
        sql_event->add_view_name(view->view_name());
        expanded_views.insert(view_name);
        has_new_views = true;
        
        LOG_DEBUG("expand view: %s", view_name.c_str());
      }
      
      if(!has_new_views) {
        break; // 没有新的视图需要展开，结束循环
      }

      // 如果有新视图被展开，需要重新解析
      if (has_new_views) {
        rc = parse_stage_.handle_view_request(sql_event);
        if (OB_FAIL(rc)) {
          LOG_TRACE("failed to parse view. rc=%s", strrc(rc));
          return rc;
        }
      }
    }
    
    LOG_DEBUG("total expanded %zu views", expanded_views.size());
  }
  
  rc = resolve_stage_.handle_request(sql_event);
  if (OB_FAIL(rc)) {
    LOG_TRACE("failed to do resolve. rc=%s", strrc(rc));
    return rc;
  }

  // 生成逻辑计划、查询优化，根据逻辑计划生成物理算子
  rc = optimize_stage_.handle_request(sql_event);
  if (rc != RC::UNIMPLEMENTED && rc != RC::SUCCESS) {
    LOG_TRACE("failed to do optimize. rc=%s", strrc(rc));
    return rc;
  }

  rc = execute_stage_.handle_request(sql_event);
  if (OB_FAIL(rc)) {
    LOG_TRACE("failed to do execute. rc=%s", strrc(rc));
    return rc;
  }

  return rc;
}