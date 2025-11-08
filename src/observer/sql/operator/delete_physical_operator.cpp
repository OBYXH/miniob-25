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
// Created by WangYunlai on 2022/6/27.
//

#include "sql/operator/delete_physical_operator.h"
#include "common/log/log.h"
#include "storage/record/record.h"
#include "storage/table/table.h"
#include "storage/table/view.h"
#include "storage/trx/trx.h"

RC DeletePhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  unique_ptr<PhysicalOperator> &child = children_[0];

  RC rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  trx_ = trx;

  // 1. 先统一收集所有需要删除的记录
  unordered_set<string> affected_tables;  // 记录涉及的基表

  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);

    if (table_->is_view()) {
      if (row_tuple->cell_num() != row_tuple->rid_list_.size()) {
        LOG_PANIC("delete view: cell num is not equal to rid num");
        return RC_WITH_LOCATION(RC::INTERNAL, " ");
      }

      string delete_table_name;
      RID    delete_rid;
      // 在视图场景下，由于引入多表，需要使用tuple中的rid和table_name
      for (size_t i = 0; i < row_tuple->rid_list_.size(); i++) {
        auto tmp_table_name = row_tuple->table_name_list_[i];
        auto tmp_rid        = row_tuple->rid_list_[i];

        LOG_DEBUG("delete from view base table: %s, rid: %s", 
                  tmp_table_name.c_str(), tmp_rid.to_string().c_str());

        // 记录涉及的基表, 表达式列除外
        if (tmp_table_name.empty()) {
          LOG_WARN("delete from view: base table name is empty, skip expression column");
          continue;
        }
        affected_tables.insert(tmp_table_name);
        // 记录一次即可
        if (delete_table_name.empty()) {
          delete_table_name = tmp_table_name;
          delete_rid        = tmp_rid;
        }
      }
      // 视图删除操作只允许涉及单表
      if (affected_tables.size() > 1) {
        LOG_WARN("Cannot delete from view: operation involves %zu base tables, only single table operations are allowed",
                affected_tables.size());
        return RC::MULTIPLE_BASE_TABLES;
      }
      // Safety: 前面已经保证了基表唯一，只需要获取一个有效的RID即可
      records_.push_back(row_tuple->record());
      record_table_names_.push_back(delete_table_name);
      record_rids_.push_back(delete_rid);
    } else {
      // 普通表的情况
      string raw_table_name = tuple->raw_table_name();
      RID    raw_rid        = tuple->raw_rid();

      records_.push_back(row_tuple->record());
      record_table_names_.push_back(raw_table_name);
      record_rids_.push_back(raw_rid);

      affected_tables.insert(raw_table_name);
    }
  }

  child->close();

  // 3. 构建基表映射
  unordered_map<string, Table *> base_table_map;
  if (table_->is_view()) {
    auto *view        = static_cast<View *>(table_);
    auto  base_tables = view->base_tables();
    for (auto *base_table : base_tables) {
      base_table_map[base_table->name()] = base_table;
    }
  }

  // 4. 执行删除操作
  // 记录的有效性由事务来保证，如果事务不保证删除的有效性，那说明此事务类型不支持并发控制，比如VacuousTrx
  for (size_t idx = 0; idx < records_.size(); idx++) {
    auto &raw_table_name = record_table_names_[idx];
    auto &raw_rid        = record_rids_[idx];

    Table *delete_table = table_;

    // 选择删除的表
    if (table_->is_view()) {
      if (raw_table_name.empty()) {
        LOG_WARN("delete from view: raw table name is empty");
        continue;
      }

      auto it = base_table_map.find(raw_table_name);
      if (it == base_table_map.end()) {
        LOG_WARN("delete from view: cannot find base table: %s", raw_table_name.c_str());
        continue;
      }
      delete_table = it->second;
    }

    // 从实际的表中读取记录
    Record actual_record;
    rc = delete_table->get_record(raw_rid, actual_record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to get record from table %s, rid=%s", 
               delete_table->name(), raw_rid.to_string().c_str());
      return rc;
    }

    // 执行删除
    rc = trx_->delete_record(delete_table, actual_record);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to delete record from table %s: %s", 
               delete_table->name(), strrc(rc));
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC DeletePhysicalOperator::next() { return RC::RECORD_EOF; }

RC DeletePhysicalOperator::close() { return RC::SUCCESS; }
