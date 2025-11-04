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
// Created by wangyunlai.wyl on 2021/5/19.
//

#include <random>

#include "storage/index/ivfflat_index.h"
#include "sql/builtin/builtin.h"
#include <filesystem>

#define MAX_ITERATIONS 15

using Vector = std::vector<float>;

void vector_add(Vector &a, const Vector &b)
{
  for (size_t i = 0; i < a.size(); i++) {
    a[i] += b[i];
  }
}

void vector_scalar_div(Vector &a, float x)
{
  for (auto &y : a) {
    y /= x;
  }
}

// 计算两个向量之间的距离
float compute_distance(const Vector &a, const Vector &b, NormalFunctionType dist_fn)
{
  switch (dist_fn) {
    case NormalFunctionType::L2_DISTANCE: return builtin::l2_distance(a, b);
    case NormalFunctionType::COSINE_DISTANCE: return builtin::cosine_distance(a, b);
    case NormalFunctionType::INNER_PRODUCT: return builtin::inner_product(a, b);
    default: return 0;
  }
}

// 找到离给定向量最近的质心
auto find_centroid(const Vector &vec, const std::vector<Vector> &centroids, NormalFunctionType dist_fn) -> size_t
{
  size_t best_index   = 0;
  float  min_distance = std::numeric_limits<float>::max();
  for (size_t i = 0; i < centroids.size(); i++) {
    float distance = compute_distance(vec, centroids[i], dist_fn);
    if (distance < min_distance) {
      min_distance = distance;
      best_index   = i;
    }
  }
  return best_index;
}

// 根据数据和旧质心计算新质心
auto find_centroids(const std::vector<std::pair<Vector, RID>> &data, const std::vector<Vector> &centroids,
    NormalFunctionType dist_fn) -> std::vector<Vector>
{
  std::vector<Vector> new_centroids(centroids.size(), Vector(centroids[0].size(), 0.0));
  std::vector<size_t> counts(centroids.size(), 0);
  
  // ✅ 同时记录每个簇的点的索引
  std::vector<std::vector<size_t>> cluster_indices(centroids.size());

  // 聚类点累加
  for (size_t idx = 0; idx < data.size(); ++idx) {
    const auto &[vec, rid] = data[idx];
    size_t cluster = find_centroid(vec, centroids, dist_fn);
    vector_add(new_centroids[cluster], vec);
    counts[cluster]++;
    cluster_indices[cluster].push_back(idx);
  }

  // 计算平均值作为新质心
  for (size_t i = 0; i < new_centroids.size(); i++) {
    if (counts[i] > 0) {
      vector_scalar_div(new_centroids[i], counts[i]);
    } else {
      // ✅ 处理空簇：从最大簇中分裂一个点
      LOG_WARN("Empty cluster %lu detected, reinitializing", i);
      
      // 找到最大的簇
      size_t max_cluster = 0;
      size_t max_count = 0;
      for (size_t j = 0; j < counts.size(); j++) {
        if (counts[j] > max_count) {
          max_count = counts[j];
          max_cluster = j;
        }
      }
      
      if (max_count > 0 && !cluster_indices[max_cluster].empty()) {
        // ✅ 从最大簇中随机选一个点
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> dis(0, cluster_indices[max_cluster].size() - 1);
        size_t selected_idx = cluster_indices[max_cluster][dis(gen)];
        new_centroids[i] = data[selected_idx].first;
        
        LOG_INFO("Reinitialized empty cluster %lu from cluster %lu (selected from %lu points)", 
                 i, max_cluster, cluster_indices[max_cluster].size());
      } else {
        // Fallback: 保持旧质心或随机选择
        if (!data.empty()) {
          std::mt19937 gen(std::random_device{}());
          std::uniform_int_distribution<> dis(0, data.size() - 1);
          new_centroids[i] = data[dis(gen)].first;
        } else {
          new_centroids[i] = centroids[i];
        }
      }
    }
  }
  
  return new_centroids;
}

RC IvfflatIndex::create(Table *table, const char *file_name, const IndexMeta &index_meta)
{
  if (inited_) {
    LOG_WARN("Failed to create index due to the index has been created before. file_name:%s, index:%s",
        file_name, index_meta.to_string().c_str());
    return RC::RECORD_OPENNED;
  }

  Index::init(index_meta);

  inited_     = true;
  table_      = table;
  index_file_ = file_name; 
  field_meta_ = index_meta.fields()[0];
  LOG_INFO("Successfully create index, file_name:%s, index:%s",
    file_name, index_meta.to_string().c_str());
  return RC::SUCCESS;
}

RC IvfflatIndex::open(Table *table, const char *file_name, const IndexMeta &index_meta)
{
  if (inited_) {
    LOG_WARN("Index already opened: %s", file_name);
    return RC::RECORD_OPENNED;
  }

  Index::init(index_meta);

  table_      = table;
  index_file_ = file_name;
  field_meta_ = index_meta.fields()[0];

  // ✅ 从磁盘加载索引
  RC rc = load_from_disk();
  if (rc != RC::SUCCESS) {
    LOG_WARN("Failed to load index from disk: %s, rc=%s", file_name, strrc(rc));
    return rc;
  }

  inited_ = true;
  LOG_INFO("Successfully opened index: %s", file_name);
  return RC::SUCCESS;
}

RC IvfflatIndex::close() { 
  if (!inited_) {
    return RC::SUCCESS;
  }

  // 关闭前同步到磁盘
  RC rc = sync();
  if (rc != RC::SUCCESS) {
    LOG_WARN("Failed to sync index before close: %s", strrc(rc));
  }

  inited_ = false;
  return RC::SUCCESS; 
}

RC IvfflatIndex::build_index(
    std::vector<std::pair<Vector, RID>> &initial_data)
{
  // 初始化距离度量函数
  distance_fn_ = NormalFunctionType::COSINE_DISTANCE;

  if (initial_data.empty()) {
    return RC::SUCCESS;
  }

  std::mt19937 gen(std::random_device{}());
  centroids_.resize(lists_);

  // ✅ K-means++ 初始化
  std::uniform_int_distribution<> dis(0, initial_data.size() - 1);
  
  // 第一个质心：随机选择
  centroids_[0] = initial_data[dis(gen)].first;

  // 后续质心：选择离现有质心最远的点
  for (int i = 1; i < lists_; i++) {
    std::vector<float> min_distances(initial_data.size(), std::numeric_limits<float>::max());
    
    // 计算每个点到最近质心的距离
    for (size_t j = 0; j < initial_data.size(); j++) {
      for (int k = 0; k < i; k++) {
        float dist = compute_distance(initial_data[j].first, centroids_[k], distance_fn_);
        min_distances[j] = std::min(min_distances[j], dist);
      }
    }
    
    // ✅ 按概率选择下一个质心（距离越远，概率越大）
    std::discrete_distribution<> prob_dist(min_distances.begin(), min_distances.end());
    size_t next_idx = prob_dist(gen);
    centroids_[i] = initial_data[next_idx].first;
  }

  // ✅ K-means 迭代（优化收敛条件）
  size_t cnt = 0;
  bool changed = true;
  float threshold = 0.001;  // 相对于维度调整
  
  while (changed && cnt < MAX_ITERATIONS) {
    changed = false;
    auto new_centroids = find_centroids(initial_data, centroids_, distance_fn_);

    for (int i = 0; i < lists_; i++) {
      float change = compute_distance(new_centroids[i], centroids_[i], distance_fn_);
      
      if (change > threshold) {
        changed = true;
      }
      centroids_[i] = std::move(new_centroids[i]);
    }

    ++cnt;
  }
  
  LOG_INFO("K-means converged in %lu iterations", cnt);

  // 将数据点分配到最近的质心
  centroids_buckets_.resize(lists_);
  for (auto &[vec, rid] : initial_data) {
    size_t index = find_centroid(vec, centroids_, distance_fn_);
    centroids_buckets_[index].emplace_back(std::move(vec), rid);
  }

  // ✅ 打印聚类统计
  for (size_t i = 0; i < centroids_buckets_.size(); i++) {
    LOG_INFO("Cluster %lu: %lu vectors", i, centroids_buckets_[i].size());
  }

  return RC::SUCCESS;
}

std::vector<RID> IvfflatIndex::ann_search(const std::vector<float> &base_vector, size_t limit)
{
  std::vector<std::pair<float, size_t>> centroid_distances;

  // 计算与所有簇中心的距离
  for (size_t i = 0; i < centroids_.size(); i++) {
    float dist = compute_distance(base_vector, centroids_[i], distance_fn_);
    centroid_distances.emplace_back(dist, i);
  }

  // 找到最近的 probes_ 个簇
  std::sort(centroid_distances.begin(),
      centroid_distances.end(),
      [&](const std::pair<float, size_t> &a, const std::pair<float, size_t> &b) {
        return a.first < b.first;  // 按距离升序排序
      });

  std::vector<std::pair<float, RID>> candidates;

  // 探测最近的 probes_ 个簇
  for (int i = 0; i < probes_; i++) {
    size_t cluster_index = centroid_distances[i].second;
    for (const auto &[vec, rid] : centroids_buckets_[cluster_index]) {
      float dist = compute_distance(base_vector, vec, distance_fn_);
      candidates.emplace_back(dist, rid);
    }
  }

  // 根据距离排序并选取前 limit 个结果
  std::sort(candidates.begin(), candidates.end(), [&](const std::pair<float, RID> &a, const std::pair<float, RID> &b) {
    return a.first < b.first;  // 按距离升序排序
  });

  auto             size = std::min(candidates.size(), limit);
  std::vector<RID> result(size);
  for (size_t i = 0; i < size; i++) {
    result[i] = candidates[i].second;
  }

  return result;
}

Vector IvfflatIndex::get_vector(const char *record)
{
  int    size = field_meta_.len() / sizeof(float);
  Vector vector(size);
  // 直接将记录位置调整为适当的浮点数指针
  const float *data = reinterpret_cast<const float *>(record + field_meta_.offset());
  // 使用标准库函数来进行快速的内存复制
  std::copy(data, data + size, vector.begin());
  return vector;
}

RC IvfflatIndex::insert_entry(const char *record, const RID *rid)
{
  Vector vector = get_vector(record);
  size_t index  = find_centroid(vector, centroids_, this->distance_fn_);
  centroids_buckets_[index].emplace_back(std::move(vector), *rid);
  return RC::SUCCESS;
}

RC IvfflatIndex::delete_entry(const char *record, const RID *rid) { return RC::SUCCESS; }

RC IvfflatIndex::sync() { 
  if (!inited_) {
    return RC::SUCCESS;
  }

  // ✅ 保存到磁盘
  RC rc = save_to_disk();
  if (rc != RC::SUCCESS) {
    LOG_ERROR("Failed to save index to disk: %s", strrc(rc));
    return rc;
  }

  LOG_INFO("Successfully synced index: %s", index_file_.c_str());
  return RC::SUCCESS;
}

RC IvfflatIndex::save_to_disk() {
  IvfflatFileWriter writer;
  
  RC rc = writer.open(index_file_.c_str());
  if (rc != RC::SUCCESS) {
    return rc;
  }

  // ✅ 写入头部
  IvfflatFileHeader header;
  header.lists = lists_;
  header.probes = probes_;
  header.distance_fn = static_cast<uint32_t>(distance_fn_);
  header.dimension = centroids_.empty() ? 0 : centroids_[0].size();
  
  uint64_t total = 0;
  for (const auto &bucket : centroids_buckets_) {
    total += bucket.size();
  }
  header.total_vectors = total;

  rc = writer.write_header(header);
  if (rc != RC::SUCCESS) {
    writer.close();
    return rc;
  }

  // ✅ 写入质心
  rc = writer.write_centroids(centroids_);
  if (rc != RC::SUCCESS) {
    writer.close();
    return rc;
  }

  // ✅ 写入每个 bucket
  for (size_t i = 0; i < centroids_buckets_.size(); ++i) {
    rc = writer.write_bucket(i, centroids_buckets_[i]);
    if (rc != RC::SUCCESS) {
      writer.close();
      return rc;
    }
  }

  writer.close();
  return RC::SUCCESS;
}

RC IvfflatIndex::load_from_disk() {
  // ✅ 检查文件是否存在
  if (!std::filesystem::exists(index_file_)) {
    LOG_WARN("Index file does not exist: %s", index_file_.c_str());
    return RC::IOERR_OPEN;
  }

  IvfflatFileReader reader;
  
  RC rc = reader.open(index_file_.c_str());
  if (rc != RC::SUCCESS) {
    return rc;
  }

  // ✅ 读取头部
  IvfflatFileHeader header;
  rc = reader.read_header(header);
  if (rc != RC::SUCCESS) {
    reader.close();
    return rc;
  }

  lists_ = header.lists;
  probes_ = header.probes;
  distance_fn_ = static_cast<NormalFunctionType>(header.distance_fn);

  // ✅ 读取质心
  rc = reader.read_centroids(centroids_, header.dimension);
  if (rc != RC::SUCCESS) {
    reader.close();
    return rc;
  }

  // ✅ 读取所有 bucket
  centroids_buckets_.resize(lists_);
  for (int i = 0; i < lists_; ++i) {
    rc = reader.read_bucket(i, centroids_buckets_[i], header.dimension);
    if (rc != RC::SUCCESS) {
      reader.close();
      return rc;
    }
  }

  reader.close();
  LOG_INFO("Loaded %lu vectors from %lu buckets", header.total_vectors, lists_);
  return RC::SUCCESS;
}
