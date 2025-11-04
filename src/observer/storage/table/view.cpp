#include "storage/record/record_manager.h"
#include "storage/record/physical_op_record_scanner.h"
#include <storage/table/view.h>
#include <common/types.h>

void View::init_table_meta(const vector<FieldMeta> &fields)
{
  std::vector<AttrInfoSqlNode> attr_infos;
  for (const auto &field : fields) {
    AttrInfoSqlNode attr_info;
    attr_info.nullable = field.nullable();
    attr_info.name     = field.name();
    attr_info.type     = field.type();
    attr_info.length = field.len();
    attr_infos.push_back(attr_info);
  }
  table_meta_.init(view_id_, view_name_.c_str(), nullptr, attr_infos, {}, StorageFormat::ROW_FORMAT);
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