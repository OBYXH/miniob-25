#pragma once

#include <vector>
#include <memory>
#include <fstream>
#include <queue>
#include "common/sys/rc.h"
#include "common/value.h"
#include "sql/parser/parse_defs.h"

/**
 * @brief 外排序工具类，用于大数据量排序
 *
 * 采用多路归并排序算法：
 * 1. 分块读取数据到内存，排序后写入临时文件（run文件）
 * 2. 使用K路归并将所有run文件合并输出
 */
class ExternalSorter
{
public:
  /**
   * @param order_by 排序字段和方向
   * @param max_memory_bytes 最大内存使用量（字节）
   */
  ExternalSorter(std::vector<bool> ascs, size_t max_memory_bytes);
  ~ExternalSorter();

  /**
   * @brief 添加一行数据
   * @param order_values 排序字段的值
   * @param result_values 结果字段的值
   */
  RC add_row(const std::vector<Value> &order_values, const std::vector<Value> &result_values);

  /**
   * @brief 完成数据添加，进行排序
   */
  RC finish_add();

  /**
   * @brief 获取下一行排序后的结果
   * @param result_values 输出参数，返回结果字段的值
   * @return RC::SUCCESS 成功，RC::RECORD_EOF 无更多数据
   */
  RC next(std::vector<Value> &result_values);

  /**
   * @brief 清理资源
   */
  void cleanup();

private:
  struct Row
  {
    std::vector<Value> order_values;   // 用于排序的字段值
    std::vector<Value> result_values;  // 最终输出的字段值
  };

  struct RunFile
  {
    std::string   filename;
    std::ifstream stream;
    Row           current_row;
    bool          has_data;
    size_t        run_index;  // 用于堆排序时的稳定性
  };

  struct RunCompare
  {
    ExternalSorter *sorter;

    bool operator()(const std::shared_ptr<RunFile> &a, const std::shared_ptr<RunFile> &b) const;
  };

  /**
   * @brief 比较两行数据的排序字段
   * @return <0: a<b, 0: a==b, >0: a>b
   */
  int compare_rows(const std::vector<Value> &a, const std::vector<Value> &b) const;

  /**
   * @brief 将当前内存中的数据排序并写入run文件
   */
  RC flush_current_run();

  /**
   * @brief 估算一行数据占用的内存字节数
   */
  size_t estimate_row_bytes(const Row &row) const;

  /**
   * @brief 序列化一行数据到流
   */
  RC serialize_row(std::ofstream &out, const Row &row) const;

  /**
   * @brief 从流反序列化一行数据
   */
  RC deserialize_row(std::ifstream &in, Row &row) const;

  /**
   * @brief 生成临时文件名
   */
  std::string generate_temp_filename();

  /**
   * @brief 从run文件读取下一行
   */
  RC read_next_from_run(std::shared_ptr<RunFile> run);

private:
  std::vector<bool> ascs_;  // 每个排序字段的升降序标志
  size_t            max_memory_bytes_;
  size_t            current_memory_bytes_;

  std::vector<Row>         current_run_;    // 当前内存中的数据
  std::vector<std::string> run_filenames_;  // 所有run文件名

  // K路归并使用的优先队列
  std::priority_queue<std::shared_ptr<RunFile>, std::vector<std::shared_ptr<RunFile>>, RunCompare> merge_heap_;

  bool        finished_add_;
  size_t      next_run_index_;
  std::string temp_dir_;
};