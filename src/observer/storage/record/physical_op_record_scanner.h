#include "storage/record/record_manager.h"
#include "sql/operator/physical_operator.h"
#include "storage/record/record_scanner.h"
#include <cassert>

class PhysicalOperator;

class RecordPhysicalOperatorScanner : RecordScanner
{
public:
  RecordPhysicalOperatorScanner() = default;
  ~RecordPhysicalOperatorScanner() = default;
  RC open_scan() {
    // should not be called
    assert(false);
    return RC_WITH_LOCATION(RC::INTERNAL, "");
  }
  RC open_oper(Trx *trx);
  RC close_scan();
  RC next(Record &record);
  RC next_tuple();

  void set_oper(unique_ptr<PhysicalOperator> oper) { oper_ = std::move(oper); }

  Tuple *current_tuple() { return tuple; }
private:
  unique_ptr<PhysicalOperator> oper_;
  Tuple *tuple;
};
