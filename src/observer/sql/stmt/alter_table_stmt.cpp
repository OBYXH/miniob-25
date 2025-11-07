#include "sql/stmt/alter_table_stmt.h"
#include "common/lang/string.h"
#include "common/type/attr_type.h"
#include "sql/parser/parse_defs.h"
#include "storage/db/db.h"
#include "storage/field/field_meta.h"
#include "storage/table/table_meta.h"

RC AlterTableStmt::create(Db *db, AlterSqlNode &alter_sql, Stmt *&stmt)
{
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }
  const char *table_name = alter_sql.relation_name.c_str();
  if (nullptr == table_name) {
    LOG_WARN("invalid argument. table_name is null");
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC_WITH_LOCATION(RC::SCHEMA_TABLE_NOT_EXIST, "");
  }
  if (alter_sql.alter_type == AlterType::ALTER_RENAME) {
    Table *new_table = db->find_table(alter_sql.new_relation_name.c_str());
    if (nullptr != new_table && strcmp(alter_sql.new_relation_name.c_str(), table_name) != 0) {
      LOG_WARN("table already exists. db=%s, table_name=%s", db->name(), alter_sql.new_relation_name.c_str());
      return RC::SCHEMA_TABLE_EXIST;
    }
  } else if (alter_sql.alter_type == AlterType::ALTER_ADD_FULLTEXT_INDEX) {
    // 检查索引是否已存在
    if (table->table_meta().index(alter_sql.index_name.c_str()) != nullptr) {
      LOG_WARN("index already exists. table=%s, index=%s", table_name, alter_sql.index_name.c_str());
      return RC::INDEX_EXIST;
    }
    // 检查字段是否存在且类型正确
    const FieldMeta *field_meta = table->table_meta().field(alter_sql.index_column.c_str());
    if (field_meta == nullptr) {
      LOG_WARN("no such field. table=%s, field=%s", table_name, alter_sql.index_column.c_str());
      return RC::SCHEMA_FIELD_NOT_EXIST;
    }
    if (field_meta->type() != AttrType::TEXTS && field_meta->type() != AttrType::CHARS) {
      LOG_WARN("fulltext index only support TEXTS or CHARS field. table=%s, field=%s, type=%d",
          table_name, alter_sql.index_column.c_str(), static_cast<int>(field_meta->type()));
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }

    // 检查分词器是否存在
    if (alter_sql.parser_name != "jieba") {
      LOG_WARN("Unsupported parser for full-text index: %s", alter_sql.parser_name.c_str());
      return RC::UNSUPPORTED;
    }
    string            new_attribute_name = alter_sql.new_attribute_name;
    AttrInfoSqlNode  *attr_info          = alter_sql.old_attr_info;
    string            new_table_name     = alter_sql.new_relation_name;
    vector<FieldMeta> fields_meta;
    RC                rc = table->table_meta().get_field_metas({alter_sql.index_column}, fields_meta);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to get field metas. table name=%s", table_name);
      return rc;
    }
    stmt = new AlterTableStmt(
        table, alter_sql.alter_type, new_attribute_name, *attr_info, new_table_name, alter_sql.index_name, fields_meta);
    return RC::SUCCESS;
  }

  // 获取字段的元信息
  TableMeta   meta               = table->table_meta();
  const char *old_attribute_name = alter_sql.old_attr_info->name.c_str();
  if (!common::is_blank(old_attribute_name)) {
    auto field_meta = meta.field(old_attribute_name);
    if (field_meta == nullptr && alter_sql.alter_type != AlterType::ALTER_ADD) {
      LOG_WARN("no such field. table=%s, field=%s", table_name, old_attribute_name);
      return RC::SCHEMA_FIELD_NOT_EXIST;
    } else if (field_meta != nullptr && alter_sql.alter_type == AlterType::ALTER_ADD) {
      LOG_WARN("field already exists. table=%s, field=%s", table_name, old_attribute_name);
      return RC::SCHEMA_FIELD_REPEAT;
    }
  }
  string           new_attribute_name = alter_sql.new_attribute_name;
  AttrInfoSqlNode *attr_info          = alter_sql.old_attr_info;
  string           new_table_name     = alter_sql.new_relation_name;
  stmt = new AlterTableStmt(table, alter_sql.alter_type, new_attribute_name, *attr_info, new_table_name);
  return RC::SUCCESS;
}