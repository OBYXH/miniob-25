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

#include "sql/operator/update_physical_operator.h"
#include "common/log/log.h"
#include "common/sys/rc.h"
#include "common/type/attr_type.h"
#include "common/value.h"
#include "sql/expr/expression.h"
#include "sql/expr/tuple.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"
#include "storage/table/view.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

RC UpdatePhysicalOperator::open(Trx *trx)
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

  unordered_map<string, Table *> base_table_map;
  // 收集视图各基表的更新字段索引
  if (table_->is_view()) {
    auto *view = static_cast<View *>(table_);
    auto base_tables = view->base_tables();
    for (auto *base_table : base_tables) {
      base_table_map[base_table->name()] = base_table;

      vector<size_t> update_field_idx;
      size_t i = 0;
      for (const auto &field_meta : field_metas_) { // UPDATE 中更新的字段
        auto table_name_in_view = view->find_base_table_name(field_meta.name());
        if (table_name_in_view == base_table->name()) {
          update_field_idx.push_back(i);
        }
        i++;
      }
      selected_update_field_idx_[base_table->name()] = update_field_idx;
    }
    size_t affected_tables = 0;
    for (const auto &pair : selected_update_field_idx_) {
      if (!pair.second.empty()) {  // 该基表有字段需要更新
        affected_tables++;
      }
    }
    
    // 如果涉及多个基表，拒绝更新
    if (affected_tables > 1) {
      LOG_WARN("Cannot update view: operation involves %zu base tables, only single table operations are allowed", 
              affected_tables);
      return RC::MULTIPLE_BASE_TABLES;
    }
  } else {
    vector<size_t> update_field_idx;
    for (size_t i = 0; i < field_metas_.size(); i++) {
      update_field_idx.push_back(i);
    }
    selected_update_field_idx_[table_->name()] = update_field_idx;
  }

  trx_ = trx;

  // 统一收集所有需要更新的记录
  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }
 
    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    if (table_->is_view()) {
      
      if (row_tuple->cell_num() != row_tuple->rid_list_.size()) {
        LOG_PANIC("update view: cell num is not equal to rid num");
        return RC_WITH_LOCATION(RC::INTERNAL," ");
      }   
      // 在视图场景下，由于引入多表，需要使用tuple中的rid和table_name
      for (size_t i = 0; i < row_tuple->rid_list_.size(); i++) {
          // 在多表的情况下，rowtuple 中的 cell 可能来自不同表的 tuple，他们都有自己的 rid 和 table_name
          auto base_table_name = row_tuple->table_name_list_[i];
          auto update_rid       = row_tuple->rid_list_[i];
  
          LOG_DEBUG("we are updating base table of view: %s, rid: %s", base_table_name.c_str(), update_rid.to_string().c_str());
          records_.push_back(row_tuple->record());
          record_table_names_.push_back(base_table_name);
          record_rids_.push_back(update_rid);
      }
    } else {
      // 保存原始表信息和RID
      string raw_table_name = tuple->raw_table_name();
      RID raw_rid = tuple->raw_rid();

      records_.push_back(row_tuple->record());
      record_table_names_.push_back(raw_table_name);
      record_rids_.push_back(raw_rid);      
    }
  }
  
  // 这里需要注意，要先释放孩子节点，确保index scan获取索引页面的锁释放，否则有死锁风险
  child->close();
  
  if (rc == RC::LOCKED_CONCURRENCY_CONFLICT) {
    LOG_WARN("record is invisible");
    return rc;
  }

  // 执行更新操作
  for (size_t idx = 0; idx < records_.size(); idx++) {
    auto &old_record = records_[idx];
    auto &raw_table_name = record_table_names_[idx];
    auto &raw_rid = record_rids_[idx];
    
    Table *update_table = table_;
    vector<size_t> update_field_idx;
    
    // 选择更新的表
    if (table_->is_view()) {
      if (raw_table_name.empty()) {
        LOG_PANIC("update view: raw table name is empty, we might got failed");
        return RC_WITH_LOCATION(RC::INTERNAL, "");
      }
      if (base_table_map.find(raw_table_name) == base_table_map.end()) {
        LOG_PANIC("update view: cannot find base table: %s", raw_table_name.c_str());
        return RC_WITH_LOCATION(RC::INTERNAL, "");
      }
      update_table = base_table_map[raw_table_name];
      update_field_idx = selected_update_field_idx_[raw_table_name];
    } else {
      update_field_idx = selected_update_field_idx_[table_->name()];
    }

    Record new_record;
    new_record.set_rid(raw_rid);
    
    // 从实际的表中读取记录
    rc = update_table->get_record(raw_rid, new_record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to get record from table %s, rid=%s", update_table->name(), raw_rid.to_string().c_str());
      return rc;
    }
    
    RowTuple tuple;
    tuple.set_record(&old_record);
    tuple.set_schema(update_table, update_table->table_meta().field_metas());
    
    // 只更新需要的字段
    for (size_t i = 0; i < update_field_idx.size(); i++) {
      size_t field_idx = update_field_idx[i];
      auto field = field_metas_[field_idx];
      
      Value value;
      bool has_sub_queried_ = false;
      if (exprs_[field_idx]->type() == ExprType::SUBQUERY) {
        while (exprs_[field_idx]->get_value(tuple, value) == RC::SUCCESS) {
          if (has_sub_queried_) {
            has_sub_queried_ = false;
            rc = RC::SUB_QUERY_VALUES_DISMATCH;
            break;
          } else {
            has_sub_queried_ = true;
          }
        }
      } else {
        rc = exprs_[field_idx]->get_value(tuple, value);
      }
      
      if (OB_FAIL(rc)) {
        LOG_WARN("failed to get value from expression");
        return rc;
      }
      
      if (value.attr_type() == AttrType::UNDEFINED) {
        value.set_null(true);
      }
      
      if (value.is_null()) {
        if (!field.nullable()) {
          LOG_WARN("field is not nullable. table name:%s,field name:%s", update_table->name(), field.name());
          return RC::UNSUPPORTED_NULL_VALUE;
        }
        new_record.data()[field.offset() + field.len() - 1] = '1';
      } else {
        Value real_value = value;
        if (field.type() != value.attr_type()) {
          if (field.type() == AttrType::TEXTS && value.attr_type() == AttrType::CHARS) {
            rc = real_value.borrow_text(value);
            if (OB_FAIL(rc)) {
              LOG_WARN("failed to borrow text value. table name:%s, field name:%s, value length:%d",
                  update_table->name(), field.name(), value.length());
              break;
            }
          } else {
            rc = Value::cast_to(value, field.type(), real_value);
            if (OB_FAIL(rc)) {
              LOG_WARN("failed to cast value. table name:%s, field name:%s, value:%s",
                  update_table->name(), field.name(), value.to_string().c_str());
              return rc;
            }
          }
        }
        
        if (real_value.length() > field.len() - field.nullable()) {
          LOG_ERROR("Value length exceeds maximum allowed length for field. Field: %s", field.name());
          return RC::IOERR_TOO_LONG;
        }
        
        rc = new_record.set_field(field.offset(), field.len(), real_value);
        if (OB_FAIL(rc)) {
          LOG_WARN("failed to set field value. table name=%s, field name=%s, rc=%s",
              update_table->name(), field.name(), strrc(rc));
          return rc;
        }
      }
    }
    
    // 从实际表中读取旧记录
    Record actual_old_record;
    rc = update_table->get_record(raw_rid, actual_old_record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to get old record");
      return rc;
    }
    
    rc = trx_->update_record(update_table, actual_old_record, new_record);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to update record. table name=%s, rc=%s", update_table->name(), strrc(rc));
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next() { return RC::RECORD_EOF; }

RC UpdatePhysicalOperator::close() { return RC::SUCCESS; }
