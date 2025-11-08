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
// Created by Longda on 2021/5/2.
//

#pragma once

#include <string>
#include <memory>

/**
 * @brief 这个文件定义函数返回码/错误码(Return Code)
 * @enum RCCode
 */

#define DEFINE_RCS                       \
  DEFINE_RC(SUCCESS)                     \
  DEFINE_RC(INVALID_ARGUMENT)            \
  DEFINE_RC(INVALID_ALIAS)               \
  DEFINE_RC(UNIMPLEMENTED)               \
  DEFINE_RC(SQL_SYNTAX)                  \
  DEFINE_RC(INTERNAL)                    \
  DEFINE_RC(NOMEM)                       \
  DEFINE_RC(NOTFOUND)                    \
  DEFINE_RC(EMPTY)                       \
  DEFINE_RC(FULL)                        \
  DEFINE_RC(EXIST)                       \
  DEFINE_RC(NOT_EXIST)                   \
  DEFINE_RC(BUFFERPOOL_OPEN)             \
  DEFINE_RC(BUFFERPOOL_NOBUF)            \
  DEFINE_RC(BUFFERPOOL_INVALID_PAGE_NUM) \
  DEFINE_RC(RECORD_OPENNED)              \
  DEFINE_RC(RECORD_INVALID_RID)          \
  DEFINE_RC(RECORD_INVALID_KEY)          \
  DEFINE_RC(RECORD_DUPLICATE_KEY)        \
  DEFINE_RC(RECORD_NOMEM)                \
  DEFINE_RC(RECORD_EOF)                  \
  DEFINE_RC(RECORD_NOT_EXIST)            \
  DEFINE_RC(RECORD_INVISIBLE)            \
  DEFINE_RC(SCHEMA_DB_EXIST)             \
  DEFINE_RC(SCHEMA_DB_NOT_EXIST)         \
  DEFINE_RC(SCHEMA_DB_NOT_OPENED)        \
  DEFINE_RC(SCHEMA_TABLE_NOT_EXIST)      \
  DEFINE_RC(SCHEMA_TABLE_EXIST)          \
  DEFINE_RC(SCHEMA_FIELD_NOT_EXIST)      \
  DEFINE_RC(SCHEMA_FIELD_REPEAT)         \
  DEFINE_RC(SCHEMA_FIELD_MISSING)        \
  DEFINE_RC(SCHEMA_FIELD_TYPE_MISMATCH)  \
  DEFINE_RC(SCHEMA_INDEX_NAME_REPEAT)    \
  DEFINE_RC(SCHEMA_UNION_DISMATCH)       \
  DEFINE_RC(IOERR_READ)                  \
  DEFINE_RC(IOERR_WRITE)                 \
  DEFINE_RC(IOERR_ACCESS)                \
  DEFINE_RC(IOERR_OPEN)                  \
  DEFINE_RC(IOERR_CLOSE)                 \
  DEFINE_RC(IOERR_SEEK)                  \
  DEFINE_RC(IOERR_TOO_LONG)              \
  DEFINE_RC(IOERR_SYNC)                  \
  DEFINE_RC(INDEX_NOT_EXIST)             \
  DEFINE_RC(INDEX_EXIST)                 \
  DEFINE_RC(VECTOR_PARSE_ERROR)          \
  DEFINE_RC(DATA_TOO_LONG)               \
  DEFINE_RC(LOCKED_UNLOCK)               \
  DEFINE_RC(LOCKED_NEED_WAIT)            \
  DEFINE_RC(LOCKED_CONCURRENCY_CONFLICT) \
  DEFINE_RC(FILE_EXIST)                  \
  DEFINE_RC(FILE_NOT_EXIST)              \
  DEFINE_RC(FILE_NAME)                   \
  DEFINE_RC(FILE_BOUND)                  \
  DEFINE_RC(FILE_CREATE)                 \
  DEFINE_RC(FILE_OPEN)                   \
  DEFINE_RC(FILE_NOT_OPENED)             \
  DEFINE_RC(FILE_CLOSE)                  \
  DEFINE_RC(FILE_REMOVE)                 \
  DEFINE_RC(VARIABLE_NOT_EXISTS)         \
  DEFINE_RC(VARIABLE_NOT_VALID)          \
  DEFINE_RC(LOGBUF_FULL)                 \
  DEFINE_RC(LOG_FILE_FULL)               \
  DEFINE_RC(LOG_ENTRY_INVALID)           \
  DEFINE_RC(JSON_PARSE_FAILED)           \
  DEFINE_RC(JSON_MEMBER_MISSING)         \
  DEFINE_RC(RANGE_ERROR)                 \
  DEFINE_RC(WAL_INVALID_FILENAME)        \
  DEFINE_RC(INPUT_EOF)                   \
  DEFINE_RC(INVALID_TOKEN)               \
  DEFINE_RC(UNEXPECTED_END_OF_STRING)    \
  DEFINE_RC(SYNTAX_ERROR)                \
  DEFINE_RC(VECTOR_DIMENSION_MISMATCH)   \
  DEFINE_RC(VECTOR_NORM_ZERO)            \
  DEFINE_RC(UNSUPPORTED)                 \
  DEFINE_RC(UNSUPPORTED_NULL_VALUE)      \
  DEFINE_RC(SUB_QUERY_VALUES_DISMATCH)   \
  DEFINE_RC(MULTIPLE_BASE_TABLES)        \
  DEFINE_RC(EXPRESSION_FIELD_NOT_UPDATABLE)\

enum class RCCode
{
#define DEFINE_RC(name) name,
  DEFINE_RCS
#undef DEFINE_RC
};

/**
 * @brief 错误详情结构
 */
struct RCDetail
{
  std::string message;
  std::string file;
  int         line = 0;
  
  RCDetail() = default;
  RCDetail(const std::string &msg) : message(msg) {}
  RCDetail(const std::string &msg, const std::string &f, int l) 
    : message(msg), file(f), line(l) {}
};

/**
 * @brief 返回码类,支持携带额外信息
 */
class RC
{
public:
  // 静态常量成员,保持 RC_WITH_LOCATION(RC::INVALID_ARGUMENT, "") 的兼容性
#define DEFINE_RC(name) static constexpr RCCode name = RCCode::name;
  DEFINE_RCS
#undef DEFINE_RC

  // 默认构造为SUCCESS
  RC() : code_(RCCode::SUCCESS) {}
  
  // 从RCCode隐式构造(保持向后兼容)
  RC(RCCode code) : code_(code) {}
  
  // 带消息的构造
  RC(RCCode code, const std::string &message) 
    : code_(code), detail_(std::make_shared<RCDetail>(message)) {}
  
  // 带完整详情的构造
  RC(RCCode code, const std::string &message, const std::string &file, int line)
    : code_(code), detail_(std::make_shared<RCDetail>(message, file, line)) {}
  
  // 隐式转换为RCCode(保持向后兼容)
  operator RCCode() const { return code_; }

  // 显式转换为int(用于错误码等场景)
  int to_int() const { return static_cast<int>(code_); }  
  
  // 获取错误码
  RCCode code() const { return code_; }
  
  // 判断是否有详细信息
  bool has_detail() const { return detail_ != nullptr; }
  
  // 获取错误消息
  const char *message() const 
  { 
    return detail_ ? detail_->message.c_str() : ""; 
  }
  
  // 获取文件名
  const char *file() const 
  { 
    return detail_ ? detail_->file.c_str() : ""; 
  }
  
  // 获取行号
  int line() const 
  { 
    return detail_ ? detail_->line : 0; 
  }
  
  // 添加上下文信息
  RC &with_message(const std::string &msg)
  {
    if (!detail_) {
      detail_ = std::make_shared<RCDetail>(msg);
    } else {
      detail_->message = msg;
    }
    return *this;
  }
  
  // 比较运算符
  bool operator==(RCCode other) const { return code_ == other; }
  bool operator!=(RCCode other) const { return code_ != other; }
  bool operator==(const RC &other) const { return code_ == other.code_; }
  bool operator!=(const RC &other) const { return code_ != other.code_; }

private:
  RCCode                       code_;
  std::shared_ptr<RCDetail>    detail_;
};

// 辅助宏,用于创建带文件/行号信息的RC
#define RC_WITH_LOCATION(code, msg) \
  RC(code, msg, __FILE__, __LINE__)

extern const char *strrc(RC rc);
extern const char *strrc(RCCode rc);

extern bool OB_SUCC(RC rc);
extern bool OB_FAIL(RC rc);
extern bool OB_SUCC(RCCode rc);
extern bool OB_FAIL(RCCode rc);