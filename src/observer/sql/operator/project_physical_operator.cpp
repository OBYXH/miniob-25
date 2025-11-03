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
// Created by WangYunlai on 2022/07/01.
//

#include "sql/operator/project_physical_operator.h"
#include "common/log/log.h"
#include "common/type/attr_type.h"
#include "sql/expr/expression.h"
#include "storage/record/record.h"
#include "storage/table/table.h"

using namespace std;

ProjectPhysicalOperator::ProjectPhysicalOperator(vector<unique_ptr<Expression>> &&expressions)
    : expressions_(std::move(expressions)), tuple_(expressions_)
{}

RC ProjectPhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  PhysicalOperator *child = children_[0].get();
  if (outer_tuple != nullptr) {
    LOG_DEBUG("msg from project_phy_oper: we are in subquery");
    child->set_outer_tuple(outer_tuple);
  }
  RC rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  return RC::SUCCESS;
}

RC ProjectPhysicalOperator::next()
{
  if (children_.empty()) {
    RC rc = RC::SUCCESS;
    if (emitted_) {
      rc = RC::RECORD_EOF;
      return rc;
    }
    emitted_     = true;
    int cell_num = tuple_.cell_num();
    for (int i = 0; i < cell_num; i++) {
      // 过滤掉非计算的select
      ExprType expr_type;
      rc = tuple_.cell_type_at(i, expr_type);
      if (OB_FAIL(rc)) {
        return rc;
      }
      if (expr_type == ExprType::FIELD) {
        return RC::RECORD_EOF;
      }
      Value value;
      rc = tuple_.cell_at(i, value);
      if (OB_FAIL(rc)) {
        return rc;
      }
    }
    return RC::SUCCESS;
  }
  return children_[0]->next();
}

RC ProjectPhysicalOperator::close()
{
  if (!children_.empty()) {
    children_[0]->close();
  }
  return RC::SUCCESS;
}
Tuple *ProjectPhysicalOperator::current_tuple()
{
  if (children_.empty()) {
    return &tuple_;
  }
  // children_为空时，说明是单独的计算表达式，这里会出错，应该移到empty的判断下方
  if (children_[0]->type() == PhysicalOperatorType::ORDER_BY || children_[0]->type() == PhysicalOperatorType::LIMIT) {
    return children_[0]->current_tuple();
  }
  tuple_.set_tuple(children_[0]->current_tuple());
  tuple_.set_rid(children_[0]->current_tuple()->raw_rid());
  tuple_.set_table_name(children_[0]->current_tuple()->raw_table_name());
  return &tuple_;
}

RC ProjectPhysicalOperator::tuple_schema(TupleSchema &schema) const
{
  for (const unique_ptr<Expression> &expression : expressions_) {
    if (*(expression->field_alias()) != '\0') {
      schema.append_cell(expression->field_alias());
      continue;
    }
    schema.append_cell(expression->name());
  }
  return RC::SUCCESS;
}