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
    return RC::INVALID_ARGUMENT;
  }
  const char *table_name = alter_sql.relation_name.c_str();
  if (nullptr == table_name) {
    LOG_WARN("invalid argument. table_name is null");
    return RC::INVALID_ARGUMENT;
  }
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
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