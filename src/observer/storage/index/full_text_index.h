/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "storage/index/index.h"
#include "storage/tokenizer/tokenizer.h"
struct PostingList
{
  std::unordered_map<RID, int, RIDHash> postings;
};

struct FullTextSearchResult
{
  RID    rid;
  double score;

  bool operator>(const FullTextSearchResult &other) const { return score > other.score; }
};

/**
 * @brief fulltext 全文索引
 * @ingroup Index
 */
class FullTextIndex : public Index
{
public:
  FullTextIndex();
  virtual ~FullTextIndex() noexcept;

  RC create(Table *table, const char *file_name, const IndexMeta &index_meta) override;
  RC open(Table *table, const char *file_name, const IndexMeta &index_meta) override;

  std::vector<FullTextSearchResult> search(const std::string &query_text);

  RC close() override;

  RC insert_entry(const char *record, const RID *rid) override;
  RC delete_entry(const char *record, const RID *rid) override;

  RC sync() override;

  RC build();

private:
  void clear_index();
  void update_avg_doc_len();

private:
  bool                       inited_ = false;
  Table                     *table_  = nullptr;
  std::shared_ptr<Tokenizer> tokenizer_;

  // 倒排索引的核心数据结构
  std::unordered_map<std::string, PostingList> inverted_index_;     // 词典 + 倒排列表
  std::unordered_map<RID, int, RIDHash>        doc_lengths_;        // 每个文档的长度
  double                                       avg_doc_len_ = 0.0;  // 平均文档长度

  // BM25 参数
  const double K1 = 1.5;
  const double B  = 0.75;
};
