#pragma once

#include <string>
#include <storage/table/table.h>
#include <sql/operator/physical_operator.h>

class RecordPhysicalOperatorScanner;

class View : public Table {
 public:
  virtual ~View() = default;
  View(std::string view_name, std::string view_definition, bool is_updatable, int32_t view_id)
      : view_name_(std::move(view_name)),
        view_definition_(std::move(view_definition)),
        is_updatable_(is_updatable),
        view_id_(view_id) {set_view(true);}
  
  const std::string &view_name() const { return view_name_; }
  const std::string &view_definition() const { return view_definition_; }
  const bool is_updatable() const { return is_updatable_; }

  void init_table_meta(const vector<FieldMeta> &fields);

  void set_operator(std::unique_ptr<PhysicalOperator> oper) { operator_ = std::move(oper); }

  RC get_record_scanner(RecordPhysicalOperatorScanner &scanner, Trx *trx, ReadWriteMode mode);

  void set_base_tables(const std::vector<Table *> &tables) { base_tables_ = tables; }
  
  const std::vector<Table *> &base_tables() const { return base_tables_; }

 private:
  std::string view_name_;
  std::string view_definition_;
  bool is_updatable_;
  int32_t view_id_;

  std::unique_ptr<PhysicalOperator> operator_;
  std::vector<Table *> base_tables_;
};