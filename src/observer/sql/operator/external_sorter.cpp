#include "external_sorter.h"
#include "common/log/log.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <cstring>

using namespace std;

ExternalSorter::ExternalSorter(vector<bool> ascs, size_t max_memory_bytes)
    : ascs_(std::move(ascs)),
      max_memory_bytes_(max_memory_bytes),
      current_memory_bytes_(0),
      merge_heap_(RunCompare{this}),
      finished_add_(false),
      next_run_index_(0)
{
  // 创建临时目录
  temp_dir_ = "/tmp/miniob_sort_" + to_string(reinterpret_cast<uintptr_t>(this));
  filesystem::create_directories(temp_dir_);
}

ExternalSorter::~ExternalSorter() { cleanup(); }

void ExternalSorter::cleanup()
{
  // 清空堆
  while (!merge_heap_.empty()) {
    merge_heap_.pop();
  }

  // 删除所有临时文件
  for (const auto &filename : run_filenames_) {
    filesystem::remove(filename);
  }
  run_filenames_.clear();

  // 删除临时目录
  if (!temp_dir_.empty() && filesystem::exists(temp_dir_)) {
    filesystem::remove_all(temp_dir_);
  }
}

size_t ExternalSorter::estimate_row_bytes(const Row &row) const
{
  size_t bytes = 0;

  auto estimate_value = [](const Value &v) -> size_t {
    size_t size = sizeof(Value);
    if (v.attr_type() == AttrType::CHARS) {
      size += v.length();
    } else if (v.attr_type() == AttrType::VECTORS) {
      size += v.length();
    }
    return size;
  };

  for (const auto &v : row.order_values) {
    bytes += estimate_value(v);
  }
  for (const auto &v : row.result_values) {
    bytes += estimate_value(v);
  }

  return bytes;
}

RC ExternalSorter::add_row(const vector<Value> &order_values, const vector<Value> &result_values)
{
  if (finished_add_) {
    LOG_WARN("Cannot add row after finish_add() is called");
    return RC::INTERNAL;
  }

  Row row;
  row.order_values  = order_values;
  row.result_values = result_values;

  size_t row_bytes = estimate_row_bytes(row);

  // 如果当前run加上这一行会超过内存限制，先flush
  if (!current_run_.empty() && current_memory_bytes_ + row_bytes > max_memory_bytes_) {
    RC rc = flush_current_run();
    if (rc != RC::SUCCESS) {
      return rc;
    }
  }

  current_run_.push_back(std::move(row));
  current_memory_bytes_ += row_bytes;

  return RC::SUCCESS;
}

int ExternalSorter::compare_rows(const vector<Value> &a, const vector<Value> &b) const
{
  for (size_t i = 0; i < ascs_.size(); i++) {
    int cmp_result = a[i].compare(b[i]);

    if (cmp_result != 0) {
      // 如果是降序，反转比较结果
      return ascs_[i] ? cmp_result : -cmp_result;
    }
  }
  return 0;
}

RC ExternalSorter::flush_current_run()
{
  if (current_run_.empty()) {
    return RC::SUCCESS;
  }

  // 排序当前run
  sort(current_run_.begin(), current_run_.end(), [this](const Row &a, const Row &b) {
    return compare_rows(a.order_values, b.order_values) < 0;
  });

  // 写入临时文件
  string   filename = generate_temp_filename();
  ofstream out(filename, ios::binary);
  if (!out) {
    LOG_WARN("Failed to create temp file: %s", filename.c_str());
    return RC::IOERR_WRITE;
  }

  for (const auto &row : current_run_) {
    RC rc = serialize_row(out, row);
    if (rc != RC::SUCCESS) {
      out.close();
      return rc;
    }
  }

  out.close();
  run_filenames_.push_back(filename);

  // 清空当前run
  current_run_.clear();
  current_memory_bytes_ = 0;

  LOG_INFO("Flushed run to file: %s, total runs: %lu", filename.c_str(), run_filenames_.size());

  return RC::SUCCESS;
}

string ExternalSorter::generate_temp_filename()
{
  ostringstream oss;
  oss << temp_dir_ << "/run_" << next_run_index_++ << ".dat";
  return oss.str();
}

RC ExternalSorter::serialize_row(ofstream &out, const Row &row) const
{
  // 写入order_values数量
  uint32_t order_count = row.order_values.size();
  out.write(reinterpret_cast<const char *>(&order_count), sizeof(order_count));

  // 写入每个order value
  for (const auto &v : row.order_values) {
    // 写入类型
    AttrType type = v.attr_type();
    out.write(reinterpret_cast<const char *>(&type), sizeof(type));

    // 写入长度
    uint32_t len = v.length();
    out.write(reinterpret_cast<const char *>(&len), sizeof(len));

    // 写入数据
    if (len > 0) {
      out.write(reinterpret_cast<const char *>(v.data()), len);
    }
  }

  // 写入result_values数量
  uint32_t result_count = row.result_values.size();
  out.write(reinterpret_cast<const char *>(&result_count), sizeof(result_count));

  // 写入每个result value
  for (const auto &v : row.result_values) {
    AttrType type = v.attr_type();
    out.write(reinterpret_cast<const char *>(&type), sizeof(type));

    uint32_t len = v.length();
    out.write(reinterpret_cast<const char *>(&len), sizeof(len));

    if (len > 0) {
      out.write(reinterpret_cast<const char *>(v.data()), len);
    }
  }

  if (!out) {
    return RC::IOERR_WRITE;
  }

  return RC::SUCCESS;
}

RC ExternalSorter::deserialize_row(ifstream &in, Row &row) const
{
  // 读取order_values数量
  uint32_t order_count;
  in.read(reinterpret_cast<char *>(&order_count), sizeof(order_count));
  if (!in) {
    return RC::IOERR_READ;
  }

  row.order_values.clear();
  row.order_values.reserve(order_count);

  // 读取每个order value
  for (uint32_t i = 0; i < order_count; i++) {
    AttrType type;
    in.read(reinterpret_cast<char *>(&type), sizeof(type));

    uint32_t len;
    in.read(reinterpret_cast<char *>(&len), sizeof(len));

    vector<char> buffer(len);
    if (len > 0) {
      in.read(buffer.data(), len);
    }

    if (!in) {
      return RC::IOERR_READ;
    }

    Value v;
    v.set_type(type);
    if (len > 0) {
      v.set_data(buffer.data(), len);
    }
    row.order_values.push_back(std::move(v));
  }

  // 读取result_values数量
  uint32_t result_count;
  in.read(reinterpret_cast<char *>(&result_count), sizeof(result_count));
  if (!in) {
    return RC::IOERR_READ;
  }

  row.result_values.clear();
  row.result_values.reserve(result_count);

  // 读取每个result value
  for (uint32_t i = 0; i < result_count; i++) {
    AttrType type;
    in.read(reinterpret_cast<char *>(&type), sizeof(type));

    uint32_t len;
    in.read(reinterpret_cast<char *>(&len), sizeof(len));

    vector<char> buffer(len);
    if (len > 0) {
      in.read(buffer.data(), len);
    }

    if (!in) {
      return RC::IOERR_READ;
    }

    Value v;
    v.set_type(type);
    if (len > 0) {
      v.set_data(buffer.data(), len);
    }
    row.result_values.push_back(std::move(v));
  }

  return RC::SUCCESS;
}

RC ExternalSorter::finish_add()
{
  if (finished_add_) {
    return RC::SUCCESS;
  }

  finished_add_ = true;

  // flush最后一个run
  RC rc = flush_current_run();
  if (rc != RC::SUCCESS) {
    return rc;
  }

  // 如果没有run文件（所有数据都在内存中），直接返回
  if (run_filenames_.empty()) {
    return RC::SUCCESS;
  }

  // 初始化K路归并
  for (size_t i = 0; i < run_filenames_.size(); i++) {
    auto run       = make_shared<RunFile>();
    run->filename  = run_filenames_[i];
    run->run_index = i;
    run->stream.open(run->filename, ios::binary);

    if (!run->stream) {
      LOG_WARN("Failed to open run file: %s", run->filename.c_str());
      return RC::IOERR_READ;
    }

    rc = read_next_from_run(run);
    if (rc == RC::SUCCESS) {
      merge_heap_.push(run);
    } else if (rc != RC::RECORD_EOF) {
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC ExternalSorter::read_next_from_run(shared_ptr<RunFile> run)
{
  RC rc = deserialize_row(run->stream, run->current_row);
  if (rc == RC::SUCCESS) {
    run->has_data = true;
    return RC::SUCCESS;
  } else if (run->stream.eof()) {
    run->has_data = false;
    run->stream.close();
    return RC::RECORD_EOF;
  } else {
    run->has_data = false;
    return rc;
  }
}

bool ExternalSorter::RunCompare::operator()(const shared_ptr<RunFile> &a, const shared_ptr<RunFile> &b) const
{
  int cmp = sorter->compare_rows(a->current_row.order_values, b->current_row.order_values);
  if (cmp != 0) {
    // 小顶堆，所以返回 a > b
    return cmp > 0;
  }
  // 如果相等，使用run_index保证稳定性
  return a->run_index > b->run_index;
}

RC ExternalSorter::next(vector<Value> &result_values)
{
  if (!finished_add_) {
    LOG_WARN("Must call finish_add() before next()");
    return RC::INTERNAL;
  }

  if (merge_heap_.empty()) {
    return RC::RECORD_EOF;
  }

  // 从堆顶取出最小元素
  auto run = merge_heap_.top();
  merge_heap_.pop();

  result_values = run->current_row.result_values;

  // 从该run读取下一行
  RC rc = read_next_from_run(run);
  if (rc == RC::SUCCESS) {
    merge_heap_.push(run);
  } else if (rc != RC::RECORD_EOF) {
    return rc;
  }

  return RC::SUCCESS;
}