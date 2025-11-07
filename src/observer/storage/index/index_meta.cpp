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
// Created by Wangyunlai.wyl on 2021/5/18.
//

#include "storage/index/index_meta.h"
#include "common/lang/string.h"
#include "common/lang/vector.h"
#include "common/log/log.h"
#include "storage/field/field_meta.h"
#include "storage/table/table_meta.h"
#include "json/json.h"
#include <json/value.h>
#include <sstream>

const static Json::StaticString FIELD_NAME("name");
const static Json::StaticString FIELD_INDEX_TYPE("index_type");
const static Json::StaticString FIELD_TOTAL_LEN("fields_total_len");
const static Json::StaticString FIELD_IS_UNIQUE("is_unique");
const static Json::StaticString FIELD_FIELDS("fields");
const static Json::StaticString FIELD_OFFSETS("field_offsets");

RC IndexMeta::init(const char *name, IndexType index_type, const vector<FieldMeta> &fields, bool unique)
{
  if (common::is_blank(name)) {
    LOG_ERROR("Failed to init index, name is empty.");
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }
  name_             = name;
  index_type_       = index_type;
  fields_           = fields;
  fields_total_len_ = 0;
  is_unique_        = unique;
  for (const auto &field : fields_) {
    fields_offset_.push_back(fields_total_len_);
    fields_total_len_ += field.len();
  }
  return RC::SUCCESS;
}

string IndexMeta::to_string() const
{
  std::ostringstream oss;
  oss << "Index name: " << name_ << ", Fields: [";
  for (size_t i = 0; i < fields_.size(); ++i) {
    oss << fields_[i].name();
    if (i < fields_.size() - 1) {
      oss << ", ";
    }
  }
  oss << "], Total Length: " << fields_total_len_ << ", Is Unique: " << (is_unique_ ? "Yes" : "No");
  return oss.str();
}

void IndexMeta::to_json(Json::Value &json_value) const
{
  json_value[FIELD_NAME]       = name_;
  json_value[FIELD_INDEX_TYPE] = static_cast<int>(index_type_);
  json_value[FIELD_TOTAL_LEN]  = fields_total_len_;
  json_value[FIELD_IS_UNIQUE]  = is_unique_;

  Json::Value fields_json(Json::arrayValue);
  for (const auto &field : fields_) {
    Json::Value field_json;
    field.to_json(field_json);
    fields_json.append(field_json);
  }
  json_value[FIELD_FIELDS] = fields_json;

  Json::Value offsets_json(Json::arrayValue);
  for (const auto &offset : fields_offset_) {
    offsets_json.append(offset);
  }
  json_value[FIELD_OFFSETS] = offsets_json;
}

RC IndexMeta::from_json(const Json::Value &json_value, IndexMeta &index)
{
  if (!json_value.isMember(FIELD_NAME) || !json_value.isMember(FIELD_INDEX_TYPE) ||
      !json_value.isMember(FIELD_FIELDS) || !json_value.isMember(FIELD_IS_UNIQUE) ||
      !json_value.isMember(FIELD_OFFSETS) || !json_value.isMember(FIELD_TOTAL_LEN)) {
    LOG_DEBUG("Invalid index json value: %s", json_value.toStyledString().c_str());
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }

  index.name_             = json_value[FIELD_NAME].asString();
  index.index_type_       = static_cast<IndexType>(json_value[FIELD_INDEX_TYPE].asInt());
  index.fields_total_len_ = json_value[FIELD_TOTAL_LEN].asInt();
  index.is_unique_        = json_value[FIELD_IS_UNIQUE].asBool();

  const Json::Value &fields_json = json_value[FIELD_FIELDS];
  if (!fields_json.isArray()) {
    LOG_ERROR("Invalid index json value: %s", json_value.toStyledString().c_str());
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }
  for (const auto &field_json : fields_json) {
    FieldMeta field_meta;
    RC        rc = FieldMeta::from_json(field_json, field_meta);
    if (rc != RC::SUCCESS) {
      LOG_ERROR("Deserialize index [%s]: failed to deserialize field meta. json value=%s",
                index.name_.c_str(), field_json.toStyledString().c_str());
      return rc;
    }
    index.fields_.push_back(field_meta);
  }

  const Json::Value &offsets_json = json_value[FIELD_OFFSETS];
  if (!offsets_json.isArray()) {
    LOG_ERROR("Invalid index json value: %s", json_value.toStyledString().c_str());
    return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
  }
  for (const auto &offset_json : offsets_json) {
    if (!offset_json.isInt()) {
      LOG_ERROR("Invalid index json value: %s", json_value.toStyledString().c_str());
      return RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "");
    }
    index.fields_offset_.push_back(offset_json.asInt());
  }

  return RC::SUCCESS;
}

char *IndexMeta::make_entry_from_record(const char *record) const
{
  char *entry = new char[fields_total_len_];
  for (size_t i = 0; i < fields_.size(); i++) {
    auto &field = fields_[i];
    memcpy(entry + fields_offset_[i], record + field.offset(), field.len());
  }
  return entry;
}