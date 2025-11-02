/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "storage/table/heap_table_engine.h"
#include "common/sys/rc.h"
#include "sql/parser/parse_defs.h"
#include "storage/field/field_meta.h"
#include "storage/index/full_text_index.h"
#include "storage/record/heap_record_scanner.h"
#include "common/log/log.h"
#include "storage/index/bplus_tree_index.h"
#include "storage/common/meta_util.h"
#include "storage/db/db.h"
#include "storage/record/record.h"
#include "storage/record/record_scanner.h"
#include "storage/table/table.h"
#include "storage/table/table_meta.h"
#include "storage/trx/trx.h"
#include <cstring>

HeapTableEngine::~HeapTableEngine()
{
  if (record_handler_ != nullptr) {
    delete record_handler_;
    record_handler_ = nullptr;
  }

  if (data_buffer_pool_ != nullptr) {
    data_buffer_pool_->close_file();
    data_buffer_pool_ = nullptr;
  }

  for (vector<Index *>::iterator it = indexes_.begin(); it != indexes_.end(); ++it) {
    Index *index = *it;
    delete index;
  }
  indexes_.clear();

  LOG_INFO("Table has been closed: %s", table_meta_->name());
}
RC HeapTableEngine::insert_record(Record &record)
{
  RC rc = RC::SUCCESS;
  rc    = record_handler_->insert_record(record.data(), table_meta_->record_size(), &record.rid());
  if (rc != RC::SUCCESS) {
    LOG_ERROR("Insert record failed. table name=%s, rc=%s", table_meta_->name(), strrc(rc));
    return rc;
  }

  rc = insert_entry_of_indexes(record.data(), record.rid());
  if (rc != RC::SUCCESS) {  // 可能出现了键值重复
    RC rc2 = delete_entry_of_indexes(record.data(), record.rid(), false /*error_on_not_exists*/);
    if (rc2 != RC::SUCCESS) {
      LOG_ERROR("Failed to rollback index data when insert index entries failed. table name=%s, rc=%d:%s",
                table_meta_->name(), rc2, strrc(rc2));
    }
    rc2 = record_handler_->delete_record(&record.rid());
    if (rc2 != RC::SUCCESS) {
      LOG_PANIC("Failed to rollback record data when insert index entries failed. table name=%s, rc=%d:%s",
                table_meta_->name(), rc2, strrc(rc2));
    }
  }
  return rc;
}

RC HeapTableEngine::insert_chunk(const Chunk &chunk)
{
  RC rc = RC::SUCCESS;
  rc    = record_handler_->insert_chunk(chunk, table_meta_->record_size());
  if (rc != RC::SUCCESS) {
    LOG_ERROR("Insert chunk failed. table name=%s, rc=%s", table_meta_->name(), strrc(rc));
    return rc;
  }

  // TODO: insert chunk support update index
  return rc;
}

RC HeapTableEngine::visit_record(const RID &rid, function<bool(Record &)> visitor)
{
  return record_handler_->visit_record(rid, visitor);
}

RC HeapTableEngine::get_record(const RID &rid, Record &record)
{
  RC rc = record_handler_->get_record(rid, record);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to visit record. rid=%s, table=%s, rc=%s", rid.to_string().c_str(), table_meta_->name(), strrc(rc));
    return rc;
  }

  return rc;
}

RC HeapTableEngine::delete_record(const Record &record)
{
  RC rc = RC::SUCCESS;
  for (Index *index : indexes_) {
    rc = index->delete_entry(record.data(), &record.rid());
    ASSERT(RC::SUCCESS == rc, 
           "failed to delete entry from index. table name=%s, index name=%s, rid=%s, rc=%s",
           table_meta_->name(), index->index_meta().name(), record.rid().to_string().c_str(), strrc(rc));
  }
  rc = record_handler_->delete_record(&record.rid());
  return rc;
}

RC HeapTableEngine::update_record(const Record &old_record, const Record &new_record)
{
  RC rc = RC::SUCCESS;
  for (Index *index : indexes_) {
    rc = index->delete_entry(old_record.data(), &old_record.rid());
    ASSERT(rc==RC::SUCCESS, "failed to delete entry from index. table name=%s, index name=%s, rid=%s, rc=%s",
           table_->name(), index->index_meta().name(), old_record.rid().to_string().c_str(), strrc(rc));
  }
  rc = insert_entry_of_indexes(new_record.data(), new_record.rid());
  // 键重复了
  if (rc != RC::SUCCESS) {
    // 回滚
    RC delete_rc = delete_entry_of_indexes(new_record.data(), new_record.rid(), false);
    if (delete_rc != RC::SUCCESS) {
      LOG_ERROR("failed to delete index data when update record failed. table name=%s, rc=%s", table_->name(), strrc(rc));
      return delete_rc;
    }
    RC insert_rc = insert_entry_of_indexes(old_record.data(), old_record.rid());
    if (insert_rc != RC::SUCCESS) {
      LOG_WARN("failed to rollback index data when update record failed. table name=%s, rc=%s", table_->name(), strrc(insert_rc));
      return insert_rc;
    }
    return rc;
  }
  rc = record_handler_->update_record(new_record.data(), &new_record.rid());
  return rc;
}

RC HeapTableEngine::get_record_scanner(RecordScanner *&scanner, Trx *trx, ReadWriteMode mode)
{
  scanner = new HeapRecordScanner(table_, *data_buffer_pool_, trx, db_->log_handler(), mode, nullptr);
  RC rc   = scanner->open_scan();
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to open scanner. rc=%s", strrc(rc));
  }
  return rc;
}

RC HeapTableEngine::get_chunk_scanner(ChunkFileScanner &scanner, Trx *trx, ReadWriteMode mode)
{
  RC rc = scanner.open_scan_chunk(table_, *data_buffer_pool_, db_->log_handler(), mode);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to open scanner. rc=%s", strrc(rc));
  }
  return rc;
}

RC HeapTableEngine::create_index(
    Trx *trx, IndexType index_type, const vector<FieldMeta> &field_meta, const char *index_name, bool unique)
{
  if (common::is_blank(index_name)) {
    LOG_INFO("Invalid input arguments, table name is %s, index_name is blank or attribute_name is blank", table_meta_->name());
    return RC::INVALID_ARGUMENT;
  }

  IndexMeta new_index_meta;

  RC rc = new_index_meta.init(index_name, index_type, field_meta, unique);
  if (rc != RC::SUCCESS) {
    LOG_INFO("Failed to init IndexMeta in table:%s, index_name:%s", 
             table_meta_->name(), index_name);
    return rc;
  }

  // 创建索引相关数据

  Index *index;
  switch (index_type) {
    case IndexType::BPlusTreeIndex: {
      index = new BplusTreeIndex();
    } break;
    case IndexType::FullTextIndex: {
      index = new FullTextIndex();
    } break;
  }
  string index_file = table_index_file(db_->path().c_str(), table_meta_->name(), index_name);

  rc = index->create(table_, index_file.c_str(), new_index_meta);
  if (rc != RC::SUCCESS) {
    delete index;
    LOG_ERROR("Failed to create index. file name=%s, rc=%d:%s", index_file.c_str(), rc, strrc(rc));
    return rc;
  }

  // 遍历当前的所有数据，插入这个索引
  RecordScanner *scanner = nullptr;
  rc                     = get_record_scanner(scanner, trx, ReadWriteMode::READ_ONLY);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create scanner while creating index. table=%s, index=%s, rc=%s", 
             table_meta_->name(), index_name, strrc(rc));
    return rc;
  }

  Record record;
  while (OB_SUCC(rc = scanner->next(record))) {
    rc = index->insert_entry(record.data(), &record.rid());
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to insert record into index while creating index. table=%s, index=%s, rc=%s",
               table_meta_->name(), index_name, strrc(rc));
      return rc;
    }
  }
  if (RC::RECORD_EOF == rc) {
    rc = RC::SUCCESS;
  } else {
    LOG_WARN("failed to insert record into index while creating index. table=%s, index=%s, rc=%s",
             table_meta_->name(), index_name, strrc(rc));
    return rc;
  }
  scanner->close_scan();
  delete scanner;
  LOG_INFO("inserted all records into new index. table=%s, index=%s", table_meta_->name(), index_name);

  indexes_.push_back(index);

  /// 接下来将这个索引放到表的元数据中
  TableMeta new_table_meta(*table_meta_);
  rc = new_table_meta.add_index(new_index_meta);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("Failed to add index (%s) on table (%s). error=%d:%s", index_name, table_meta_->name(), rc, strrc(rc));
    return rc;
  }

  /// 内存中有一份元数据，磁盘文件也有一份元数据。修改磁盘文件时，先创建一个临时文件，写入完成后再rename为正式文件
  /// 这样可以防止文件内容不完整
  // 创建元数据临时文件
  string  tmp_file = table_meta_file(db_->path().c_str(), table_meta_->name()) + ".tmp";
  fstream fs;
  fs.open(tmp_file, ios_base::out | ios_base::binary | ios_base::trunc);
  if (!fs.is_open()) {
    LOG_ERROR("Failed to open file for write. file name=%s, errmsg=%s", tmp_file.c_str(), strerror(errno));
    return RC::IOERR_OPEN;  // 创建索引中途出错，要做还原操作
  }
  if (new_table_meta.serialize(fs) < 0) {
    LOG_ERROR("Failed to dump new table meta to file: %s. sys err=%d:%s", tmp_file.c_str(), errno, strerror(errno));
    return RC::IOERR_WRITE;
  }
  fs.close();

  // 覆盖原始元数据文件
  string meta_file = table_meta_file(db_->path().c_str(), table_meta_->name());

  int ret = rename(tmp_file.c_str(), meta_file.c_str());
  if (ret != 0) {
    LOG_ERROR("Failed to rename tmp meta file (%s) to normal meta file (%s) while creating index (%s) on table (%s). "
              "system error=%d:%s",
              tmp_file.c_str(), meta_file.c_str(), index_name, table_meta_->name(), errno, strerror(errno));
    return RC::IOERR_WRITE;
  }

  table_meta_->swap(new_table_meta);

  LOG_INFO("Successfully added a new index (%s) on the table (%s)", index_name, table_meta_->name());
  return rc;
}

RC HeapTableEngine::drop_index(const char *index_name)
{
  RC rc = RC::SUCCESS;
  if (common::is_blank(index_name)) {
    LOG_INFO("Invalid input arguments, table name is %s, index_name is blank or attribute_name is blank", table_meta_->name());
    return RC::INVALID_ARGUMENT;
  }
  auto index = find_index(index_name);
  if (index == nullptr) {
    LOG_INFO("Index (%s) not found on table (%s) ", index_name, table_meta_->name());
    return RC::INDEX_NOT_EXIST;
  }
  IndexType type = index->type();

  // 更新表的元数据，删除对应的索引信息
  TableMeta new_table_meta(*table_meta_);
  rc = new_table_meta.drop_index(index_name);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("Failed to drop index (%s) on table (%s). error=%d:%s", index_name, table_meta_->name(), rc, strrc(rc));
    return rc;
  }
  string  tmp_file = table_meta_file(db_->path().c_str(), table_meta_->name()) + ".tmp";
  fstream fs;
  fs.open(tmp_file, ios_base::out | ios_base::binary | ios_base::trunc);
  if (!fs.is_open()) {
    LOG_ERROR("Failed to open file for write. file name=%s, errmsg=%s", tmp_file.c_str(), strerror(errno));
    return RC::IOERR_OPEN;  // 删除索引中途出错，要做还原操作
  }
  if (new_table_meta.serialize(fs) < 0) {
    LOG_ERROR("Failed to dump new table meta to file: %s. sys err=%d:%s", tmp_file.c_str(), errno, strerror(errno));
    return RC::IOERR_WRITE;
  }
  fs.close();

  // 覆盖原始元数据文件
  string meta_file = table_meta_file(db_->path().c_str(), table_meta_->name());

  int ret = rename(tmp_file.c_str(), meta_file.c_str());
  if (ret != 0) {
    LOG_ERROR("Failed to rename tmp meta file (%s) to normal meta file (%s) while creating index (%s) on table (%s). "
              "system error=%d:%s",
              tmp_file.c_str(), meta_file.c_str(), index_name, table_meta_->name(), errno, strerror(errno));
    return RC::IOERR_WRITE;
  }

  table_meta_->swap(new_table_meta);
  LOG_INFO("Successfully deleted index (%s) on the table (%s)", index_name, table_meta_->name());

  // 删除索引数据文件和内存中的索引对象
  indexes_.erase(std::remove_if(indexes_.begin(),
                     indexes_.end(),
                     [&](Index *index) {
                       if (0 == strcmp(index->index_meta().name(), index_name)) {
                         index->close();
                         delete index;
                         return true;
                       }
                       return false;
                     }),
      indexes_.end());
  string index_file = table_index_file(db_->path().c_str(), table_meta_->name(), index_name);
  if (type != IndexType::FullTextIndex) {
    if (!filesystem::remove(index_file.c_str())) {
      LOG_ERROR("Failed to remove index file. file name=%s, errmsg=%s", index_file.c_str(), strerror(errno));
      return RC::IOERR_WRITE;
    }
  }
  return rc;
}

RC HeapTableEngine::flush_table_meta()
{
  // 1. 创建新的TableMeta副本并添加字段
  TableMeta new_table_meta(*table_meta_);

  // 2. 将新元数据序列化到临时文件
  string  tmp_file = table_meta_file(db_->path().c_str(), table_meta_->name()) + ".tmp";
  fstream fs;
  fs.open(tmp_file, ios_base::out | ios_base::binary | ios_base::trunc);
  if (!fs.is_open()) {
    LOG_ERROR("Failed to open file for write. file name=%s, errmsg=%s", tmp_file.c_str(), strerror(errno));
    return RC::IOERR_WRITE;
  }

  if (new_table_meta.serialize(fs) < 0) {
    LOG_ERROR("Failed to serialize table meta. file name=%s", tmp_file.c_str());
    fs.close();
    return RC::IOERR_WRITE;
  }
  fs.close();

  // 3. 原子性替换元数据文件
  string meta_file = table_meta_file(db_->path().c_str(), table_meta_->name());
  if (filesystem::exists(meta_file)) {
    filesystem::remove(meta_file);
  }
  filesystem::rename(tmp_file, meta_file);

  // 4. 更新内存中的table_meta_
  *table_meta_ = new_table_meta;
  return RC::SUCCESS;
}

RC HeapTableEngine::add_column(const AttrInfoSqlNode &attr_info, Trx *trx)
{
  RecordScanner *record_scanner;
  RC             rc = get_record_scanner(record_scanner, trx, ReadWriteMode::READ_WRITE);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to create scanner while adding column. table=%s, column=%s, rc=%s", 
             table_meta_->name(), attr_info.name.c_str(), strrc(rc));
    return rc;
  }
  vector<Record>  new_records;
  Record          record;
  vector<PageNum> modified_pages;
  while (record_scanner->next(record) == RC::SUCCESS) {
    if (std::find(modified_pages.begin(), modified_pages.end(), record.rid().page_num) == modified_pages.end()) {
      modified_pages.emplace_back(record.rid().page_num);
    }
    Record new_record;
    // 构造新记录数据
    char *buf = (char *)malloc(table_meta_->record_size() + attr_info.length);
    memcpy((char *)buf, record.data(), table_meta_->record_size());
    buf[table_meta_->record_size() + attr_info.length - 1] = '1';  // for string type
    new_record.copy_data(buf, table_meta_->record_size() + attr_info.length);
    new_record.set_rid(record.rid());
    new_records.push_back(new_record);
    // 删除旧记录
    RC rc = delete_record(record);
    if (rc != RC::SUCCESS) {
      LOG_ERROR("failed to delete record while adding column. table=%s, column=%s, rc=%s", 
               table_meta_->name(), attr_info.name.c_str(), strrc(rc));
      return rc;
    }
    free(buf);
  }
  // 更新表元数据
  table_meta_->add_field(attr_info);
  rc = record_handler_->modify_pages_header(modified_pages, table_meta_, table_->lob_handler_);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to modify page header while adding column. table=%s, column=%s, rc=%s", 
             table_meta_->name(), attr_info.name.c_str(), strrc(rc));
    return rc;
  }
  // 插入新记录
  for (auto new_record : new_records) {
    rc = insert_record(const_cast<Record &>(new_record));
    if (rc != RC::SUCCESS) {
      LOG_ERROR("failed to insert record while adding column. table=%s, column=%s, rc=%s", 
               table_meta_->name(), attr_info.name.c_str(), strrc(rc));
      return rc;
    }
  }
  // 刷新表元数据到磁盘
  rc = flush_table_meta();
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to flush table meta while adding column. table=%s, column=%s, rc=%s", 
             table_meta_->name(), attr_info.name.c_str(), strrc(rc));
    return rc;
  }
  LOG_INFO("Successfully added column. table=%s, column=%s", table_meta_->name(), attr_info.name.c_str());
  return RC::SUCCESS;
}

RC HeapTableEngine::drop_column(const AttrInfoSqlNode &attr_info, Trx *trx)
{
  RecordScanner *record_scanner;
  RC             rc = get_record_scanner(record_scanner, trx, ReadWriteMode::READ_WRITE);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to create scanner while adding column. table=%s, column=%s, rc=%s", 
             table_meta_->name(), attr_info.name.c_str(), strrc(rc));
    return rc;
  }
  vector<Record>  new_records;
  Record          record;
  vector<PageNum> modified_pages;
  // 要先删除索引，不然无法delete record
  auto index = find_index_by_field(attr_info.name.c_str());
  if (index != nullptr) {
    string name = index->index_meta().name();
    RC     rc   = drop_index(name.c_str());
    if (rc != RC::SUCCESS) {
      LOG_ERROR("failed to drop index while dropping column. table=%s, column=%s, rc=%s", 
               table_meta_->name(), attr_info.name.c_str(), strrc(rc));
      return rc;
    }
  }
  auto field_meta = table_meta_->field(attr_info.name.c_str());
  if (field_meta == nullptr) {
    LOG_ERROR("field not found. table=%s, column=%s", table_meta_->name(), attr_info.name.c_str());
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }
  while (record_scanner->next(record) == RC::SUCCESS) {
    if (std::find(modified_pages.begin(), modified_pages.end(), record.rid().page_num) == modified_pages.end()) {
      modified_pages.emplace_back(record.rid().page_num);
    }
    // 构造新记录数据
    Record new_record;
    char  *buf = (char *)malloc(table_meta_->record_size() - field_meta->len());
    memcpy(buf, record.data(), field_meta->offset());
    int offset = field_meta->offset() + field_meta->len();
    memcpy(buf + field_meta->offset(), record.data() + offset, table_meta_->record_size() - offset);
    new_record.copy_data(buf, table_meta_->record_size() - field_meta->len());
    new_record.set_rid(record.rid());
    new_records.emplace_back(new_record);
    // 删除旧记录
    RC rc = delete_record(record);
    if (rc != RC::SUCCESS) {
      LOG_ERROR("failed to delete record while adding column. table=%s, column=%s, rc=%s", 
               table_meta_->name(), attr_info.name.c_str(), strrc(rc));
      return rc;
    }
    free(buf);
  }
  // 更新表元数据
  table_meta_->drop_field(attr_info);
  rc = record_handler_->modify_pages_header(modified_pages, table_meta_, table_->lob_handler_);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to modify page header while dropping column. table=%s, column=%s, rc=%s", 
             table_meta_->name(), attr_info.name.c_str(), strrc(rc));
    return rc;
  }
  // 更新索引meta的偏移以及field
  for (auto &index : table_->table_meta_.indexes_) {
    for (auto &field_meta : index.fields_) {
      auto new_field_meta = table_meta_->field(field_meta.name());
      if (new_field_meta != nullptr) {
        field_meta = FieldMeta(new_field_meta->name(),
            new_field_meta->type(),
            new_field_meta->offset(),
            new_field_meta->len(),
            new_field_meta->visible(),
            new_field_meta->field_id(),
            new_field_meta->nullable());
      }
    }
  }
  // 插入新记录
  for (auto new_record : new_records) {
    rc = insert_record(const_cast<Record &>(new_record));
    if (rc != RC::SUCCESS) {
      LOG_ERROR("failed to insert record while dropping column. table=%s, column=%s, rc=%s", 
               table_meta_->name(), attr_info.name.c_str(), strrc(rc));
      return rc;
    }
  }
  // 刷新表元数据到磁盘
  rc = flush_table_meta();
  if (rc != RC::SUCCESS) {
    LOG_ERROR("failed to flush table meta while dropping column. table=%s, column=%s, rc=%s", 
             table_meta_->name(), attr_info.name.c_str(), strrc(rc));
    return rc;
  }
  LOG_INFO("Successfully dropped column. table=%s, column=%s", table_meta_->name(), attr_info.name.c_str());
  return RC::SUCCESS;
}

RC HeapTableEngine::change_column(const AttrInfoSqlNode &attr_info, string new_attribute_name, Trx *trx)
{
  table_meta_->change_field(attr_info, new_attribute_name);
  // 更新索引meta的偏移以及field
  for (auto &index : table_->table_meta_.indexes_) {
    for (auto &field_meta : index.fields_) {
      auto new_field_meta = table_meta_->field(field_meta.field_id());
      if (new_field_meta != nullptr) {
        field_meta = FieldMeta(new_field_meta->name(),
            new_field_meta->type(),
            new_field_meta->offset(),
            new_field_meta->len(),
            new_field_meta->visible(),
            new_field_meta->field_id(),
            new_field_meta->nullable());
      }
    }
  }
  flush_table_meta();
  return RC::SUCCESS;
}

RC HeapTableEngine::rename_table(const char *new_table_name, Trx *trx)
{
  db_->rename_table(table_->name(), new_table_name);
  // table_meta_->rename_table(new_table_name);
  // flush_table_meta();
  return RC::SUCCESS;
}

RC HeapTableEngine::insert_entry_of_indexes(const char *record, const RID &rid)
{
  RC rc = RC::SUCCESS;
  for (Index *index : indexes_) {
    rc = index->insert_entry(record, &rid);
    if (rc != RC::SUCCESS) {
      break;
    }
  }
  return rc;
}

RC HeapTableEngine::delete_entry_of_indexes(const char *record, const RID &rid, bool error_on_not_exists)
{
  RC rc = RC::SUCCESS;
  for (Index *index : indexes_) {
    RC delete_rc = index->delete_entry(record, &rid);
    if (delete_rc != RC::SUCCESS) {
      if (delete_rc == RC::RECORD_NOT_EXIST && !error_on_not_exists) {
        continue;
      } else {
        rc = delete_rc;
        break;
      }
    }
  }
  return rc;
}

RC HeapTableEngine::sync()
{
  RC rc = RC::SUCCESS;
  for (Index *index : indexes_) {
    rc = index->sync();
    if (rc != RC::SUCCESS) {
      LOG_ERROR("Failed to flush index's pages. table=%s, index=%s, rc=%d:%s",
          table_meta_->name(),
          index->index_meta().name(),
          rc,
          strrc(rc));
      return rc;
    }
  }

  rc = data_buffer_pool_->flush_all_pages();
  LOG_INFO("Sync table over. table=%s", table_meta_->name());
  return rc;
}

Index *HeapTableEngine::find_index(const char *index_name) const
{
  for (Index *index : indexes_) {
    if (0 == strcmp(index->index_meta().name(), index_name)) {
      return index;
    }
  }
  return nullptr;
}
Index *HeapTableEngine::find_index_by_field(const char *field_name) const
{
  for (const auto &index : indexes_) {
    if (index->index_meta().fields().size() == 1) {
      auto name = index->index_meta().fields().front().name();
      if (0 == strcmp(name, field_name)) {
        return index;
      }
    }
  }
  return nullptr;
}

RC HeapTableEngine::init()
{
  string data_file = table_data_file(db_->path().c_str(), table_meta_->name());

  BufferPoolManager &bpm = db_->buffer_pool_manager();
  RC                 rc  = bpm.open_file(db_->log_handler(), data_file.c_str(), data_buffer_pool_);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("Failed to open disk buffer pool for file:%s. rc=%d:%s", data_file.c_str(), rc, strrc(rc));
    return rc;
  }

  record_handler_ = new RecordFileHandler(table_meta_->storage_format());

  rc = record_handler_->init(*data_buffer_pool_, db_->log_handler(), table_meta_, table_->lob_handler_);
  if (rc != RC::SUCCESS) {
    LOG_ERROR("Failed to init record handler. rc=%s", strrc(rc));
    delete record_handler_;
    record_handler_ = nullptr;
    return rc;
  }

  return rc;
}

RC HeapTableEngine::open()
{
  RC rc = RC::SUCCESS;
  init();
  const int index_num = table_meta_->index_num();
  for (int i = 0; i < index_num; i++) {
    const IndexMeta *index_meta = table_meta_->index(i);

    if (index_meta->index_type_ == IndexType::FullTextIndex) {
      // FullTextIndex 先不支持
      continue;
    }
    BplusTreeIndex *index      = new BplusTreeIndex();
    string          index_file = table_index_file(db_->path().c_str(), table_meta_->name(), index_meta->name());

    rc = index->open(table_, index_file.c_str(), *index_meta);
    if (rc != RC::SUCCESS) {
      delete index;
      LOG_ERROR("Failed to open index. table=%s, index=%s, file=%s, rc=%s",
                table_meta_->name(), index_meta->name(), index_file.c_str(), strrc(rc));
      // skip cleanup
      //  do all cleanup action in destructive Table function.
      return rc;
    }
    indexes_.push_back(index);
  }
  return rc;
}

RC HeapTableEngine::drop()
{
  RC rc = RC::SUCCESS;
  for (Index *index : indexes_) {
    ((BplusTreeIndex *)index)->close();
    if (rc != RC::SUCCESS) {
      LOG_ERROR("Failed to drop index. table=%s, index=%s, rc=%s",
                table_meta_->name(), index->index_meta().name(), strrc(rc));
      return rc;
    }
  }
  return rc;
}
