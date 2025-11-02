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

#include <xapian.h>
#include "storage/tokenizer/tokenizer.h"
#include "common/log/log.h"

class JiebaTermGenerator : public Xapian::TermGenerator
{
public:
  JiebaTermGenerator(Tokenizer *tokenizer) : tokenizer_(tokenizer)
  {
    if (tokenizer_ == nullptr) {
      LOG_ERROR("JiebaTermGenerator received a null tokenizer!");
    }
  }

  void set_document(const Xapian::Document &doc)
  {
    Xapian::TermGenerator::set_document(doc);
    // 获取文档的原始文本数据
    std::string text = doc.get_data();
    if (tokenizer_ && !text.empty()) {
      std::vector<std::string> tokens;
      // 使用你的 JiebaTokenizer 进行分词
      tokenizer_->cut(text, tokens);
      for (const auto &token : tokens) {
        // 将分词结果注册到 Xapian
        index_text(token);
      }
    }
  }

private:
  Tokenizer *tokenizer_;
};