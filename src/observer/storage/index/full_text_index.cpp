/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Xie Meiyi
// Rewritten by Longda & Wangyunlai
//

#include "storage/index/full_text_index.h"
#include "common/sys/rc.h"
#include "storage/tokenizer/jieba_tokenizer.h"
#include "storage/table/table.h"
#include "common/log/log.h"
#include <vector>

FullTextIndex::FullTextIndex() { tokenizer_ = std::make_shared<JiebaTokenizer>(); };

FullTextIndex::~FullTextIndex() noexcept { close(); }

RC FullTextIndex::close()
{
  if (inited_) {
    clear_index();
    inited_ = false;
  }
  return RC::SUCCESS;
}

void FullTextIndex::clear_index()
{
  inverted_index_.clear();
  doc_lengths_.clear();
  avg_doc_len_ = 0.0;
}

void FullTextIndex::update_avg_doc_len()
{
  if (doc_lengths_.empty()) {
    avg_doc_len_ = 0.0;
    return;
  }
  double total_len = 0;
  for (const auto &pair : doc_lengths_) {
    total_len += pair.second;
  }
  avg_doc_len_ = total_len / doc_lengths_.size();
}

RC FullTextIndex::create(Table *table, const char *file_name, const IndexMeta &index_meta)
{
  if (inited_) {
    LOG_WARN("Failed to create index due to the index has been created before. file_name:%s, index:%s, field:%s",
            file_name, index_meta.name(), index_meta.to_string().c_str());
    return RC::RECORD_OPENNED;
  }
  Index::init(index_meta);

  table_  = table;
  inited_ = true;
  return RC::SUCCESS;
}

RC FullTextIndex::open(Table *table, const char *file_name, const IndexMeta &index_meta)
{
  if (inited_) {
    LOG_WARN("Failed to open index due to the index has been initedd before. file_name:%s, index:%s, field:%s",
            file_name, index_meta.name(), index_meta.to_string().c_str());
    return RC::RECORD_OPENNED;
  }

  Index::init(index_meta);
  table_  = table;
  inited_ = true;
  return RC::SUCCESS;
}

RC FullTextIndex::insert_entry(const char *record, const RID *rid)
{
  auto fields_meta = index_meta_.fields();
  for (size_t i = 0; i < fields_meta.size(); ++i) {
    const FieldMeta &field_meta = fields_meta[i];
    char            *entry      = new char[field_meta.len()];
    memcpy(entry, record + field_meta.offset(), field_meta.len());
    if (field_meta.nullable()) {
      bool is_null = entry[field_meta.len() - 1] == '1';
      if (is_null) {
        delete[] entry;
        continue;  // skip null fields
      }
    }

    size_t real_len = strnlen(entry, field_meta.len());
    string text(entry, real_len);
    delete[] entry;

    std::vector<std::string> tokens;
    tokenizer_->cut(text, tokens);

    if (tokens.empty()) {
      continue;
    }

    doc_lengths_[*rid] = tokens.size();

    for (const auto &token : tokens) {
      inverted_index_[token].postings[*rid]++;
    }

    update_avg_doc_len();
  }
  return RC::SUCCESS;
}

RC FullTextIndex::delete_entry(const char *record, const RID *rid)
{
  if (!inited_ || doc_lengths_.find(*rid) == doc_lengths_.end()) {
    return RC::SUCCESS;  // 记录不存在或未被索引
  }
  auto fields_meta = index_meta_.fields();
  for (size_t i = 0; i < fields_meta.size(); ++i) {
    const FieldMeta &field_meta = fields_meta[i];
    char            *entry      = new char[field_meta.len()];
    memcpy(entry, record + field_meta.offset(), field_meta.len());
    if (field_meta.nullable()) {
      bool is_null = entry[field_meta.len() - 1] == '1';
      if (is_null) {
        delete[] entry;
        continue;  // skip null fields
      }
    }
    size_t real_len = strnlen(entry, field_meta.len());
    string text(entry, real_len);
    delete[] entry;

    std::vector<std::string> tokens;
    tokenizer_->cut(text, tokens);

    for (const auto &token : tokens) {
      auto it = inverted_index_.find(token);
      if (it != inverted_index_.end()) {
        it->second.postings.erase(*rid);
        if (it->second.postings.empty()) {
          inverted_index_.erase(it);
        }
      }
    }
    doc_lengths_.erase(*rid);
    update_avg_doc_len();
    
  }
  return RC::SUCCESS;
}

std::vector<FullTextSearchResult> FullTextIndex::search(const std::string &query_text)
{
  if (!inited_ || query_text.empty())
    return {};

  std::vector<std::string> query_tokens;
  tokenizer_->cut(query_text, query_tokens);

  std::unordered_map<RID, double, RIDHash> doc_scores;
  double                                   N = doc_lengths_.size();  // 总文档数

  for (const auto &token : query_tokens) {
    auto it = inverted_index_.find(token);
    if (it == inverted_index_.end()) {
      continue;  // 词典中没有这个词
    }

    const PostingList &plist = it->second;
    double             df    = plist.postings.size();  // 包含该词的文档数
    // 计算 IDF (Inverse Document Frequency)
    double idf = log((N - df + 0.5) / (df + 0.5));

    for (const auto &[rid, tf] : plist.postings) {
      double doc_len = doc_lengths_.at(rid);
      // 计算 BM25 分数的核心部分
      double score_part = idf * (tf * (K1 + 1)) / (tf + K1 * (1 - B + B * doc_len / avg_doc_len_));
      doc_scores[rid] += score_part;
    }
  }

  std::vector<FullTextSearchResult> results;
  for (const auto &[rid, score] : doc_scores) {
    results.push_back({rid, score});
  }

  // 按分数降序排序
  std::sort(results.begin(), results.end(), std::greater<FullTextSearchResult>());

  return results;
}

RC FullTextIndex::sync() { return RC::SUCCESS; }