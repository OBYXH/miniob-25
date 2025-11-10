#include "storage/record/record_manager.h"
#include "storage/record/physical_op_record_scanner.h"
#include <cstddef>
#include <storage/table/view.h>
#include <common/types.h>

void View::init_table_meta(const vector<FieldMeta> &fields)
{
  std::vector<AttrInfoSqlNode> attr_infos;
  size_t idx = 0;
  for (const auto &field : fields) {
    AttrInfoSqlNode attr_info;
    attr_info.nullable = field.nullable();
    if (attrs_name_.empty()) {
      attr_info.name  = field.name();
    } else {
      attr_info.name  = attrs_name_[idx];
    }
    attr_info.type     = field.type();
    attr_info.length = field.len();
    attr_infos.push_back(attr_info);

    // Safety: file name不会重复
    if (attrs_name_.empty()) { 
        field_base_table_name[field.name()] = field.table_name_;
    } else {
        attr_name_2_base_table_field_name[attrs_name_[idx]] = field.name();
        field_base_table_name[attrs_name_[idx]] = field.table_name_;
    }
    idx++;

  }
  table_meta_.init(view_id_, view_name_.c_str(), nullptr, attr_infos, {}, StorageFormat::ROW_FORMAT);
  // 为每个字段设置基表名
  idx = 0;
  for (const auto &field : fields) {
    FieldMeta *field_meta = table_meta_.mut_field(idx);
    if (field_meta != nullptr) {
      field_meta->set_basetable_name(field.table_name_.c_str());
    }
    idx++;
  }
}

RC View::get_record_scanner(RecordPhysicalOperatorScanner &scanner, Trx *trx, ReadWriteMode mode)
{
  RC rc = RC::SUCCESS;
  scanner.set_oper(std::move(operator_));
  rc = scanner.open_oper(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("View: failed to open operator: %s", strrc(rc));
    return rc;
  }
  return rc;
}