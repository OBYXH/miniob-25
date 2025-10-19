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
#include "common/type/attr_type.h"
#include "common/value.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"
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

  trx_ = trx;

  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }
    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    records_.push_back(row_tuple->record());
    // auto      field     = table_->table_meta().field(attribute_name_.c_str());
    // if (field == nullptr) {
    //   LOG_WARN("no such field: %s", attribute_name_.c_str());
    //   return RC::SCHEMA_FIELD_MISSING;
    // }
    // int field_index = field.field_id();
    // row_tuple->set_cell_at(field_index, value_);
  }
  // 这里需要注意，要先释放孩子节点，确保index scan获取索引页面的锁释放，否则有死锁风险
  child->close();

  for (auto &old_record : records_) {
    Record new_record;
    new_record.new_record(old_record.len());
    new_record = old_record;
    uint32_t null_flags_data;
    memcpy(&null_flags_data, new_record.data(), table_->table_meta().null_falg_bytes());
    std::bitset<32> null_flags(null_flags_data);
    for (uint32_t i = 0; i < field_metas_.size(); i++) {
      auto field       = field_metas_[i];
      auto field_index = field.field_id();
      auto value       = *values_[i];
      if (value.is_null()) {
        if (!null_flags.test(field_index)) {
          null_flags.set(field_index);
        }
      } else {
        if (null_flags.test(field_index)) {
          null_flags.reset(field_index);
        }
        memcpy(new_record.data() + field.offset(), value.data(), std::min(value.length(), field.len()));
        if (field.type() == AttrType::CHARS && field.len() > value.length()) {
          // pad '\0' for char type
          memset(new_record.data() + field.offset() + value.length(), 0, field.len() - value.length());
        }
      }
    }
    null_flags_data = static_cast<uint32_t>(null_flags.to_ulong());
    memcpy(new_record.data(), &null_flags_data, table_->table_meta().null_falg_bytes());
    rc = trx_->update_record(table_, old_record, new_record);
  }

  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next() { return RC::RECORD_EOF; }

RC UpdatePhysicalOperator::close() { return RC::SUCCESS; }
