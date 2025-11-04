#pragma once

#include <vector>
#include <fstream>
#include "storage/record/record.h"

/**
 * @brief IVFFlat 索引文件格式
 * @details 文件结构：
 * 
 * [Header]
 * - magic_number (4 bytes): 文件标识 "IVFF"
 * - version (4 bytes): 版本号
 * - lists (4 bytes): 质心数量
 * - probes (4 bytes): 探测数量
 * - distance_fn (4 bytes): 距离函数类型
 * - dimension (4 bytes): 向量维度
 * - total_vectors (8 bytes): 总向量数
 * 
 * [Centroids Section]
 * - centroid_count (4 bytes)
 * - for each centroid:
 *   - dimension * sizeof(float): 质心坐标
 * 
 * [Buckets Section]
 * - for each bucket (lists 个):
 *   - bucket_size (4 bytes): 当前 bucket 中的向量数量
 *   - for each vector:
 *     - dimension * sizeof(float): 向量数据
 *     - RID (sizeof(RID)): 记录位置
 */

struct IvfflatFileHeader {
  char     magic[4] = {'I', 'V', 'F', 'F'};  // 魔数
  uint32_t version = 1;                       // 版本号
  uint32_t lists;                             // 质心数量
  uint32_t probes;                            // 探测数量
  uint32_t distance_fn;                       // 距离函数
  uint32_t dimension;                         // 向量维度
  uint64_t total_vectors;                     // 总向量数

  bool validate() const {
    return magic[0] == 'I' && magic[1] == 'V' && 
           magic[2] == 'F' && magic[3] == 'F';
  }
};

class IvfflatFileWriter {
public:
  RC open(const char *filename);
  RC write_header(const IvfflatFileHeader &header);
  RC write_centroids(const std::vector<std::vector<float>> &centroids);
  RC write_bucket(size_t bucket_idx, const std::vector<std::pair<std::vector<float>, RID>> &vectors);
  RC close();

private:
  std::ofstream file_;
  std::string   filename_;
};

class IvfflatFileReader {
public:
  RC open(const char *filename);
  RC read_header(IvfflatFileHeader &header);
  RC read_centroids(std::vector<std::vector<float>> &centroids, uint32_t dimension);
  RC read_bucket(size_t bucket_idx, std::vector<std::pair<std::vector<float>, RID>> &vectors, 
                 uint32_t dimension);
  RC close();

private:
  std::ifstream file_;
  std::string   filename_;
  std::streampos centroids_offset_;
  std::streampos buckets_offset_;
};