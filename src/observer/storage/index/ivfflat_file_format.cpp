#include "storage/index/ivfflat_file_format.h"
#include "common/log/log.h"

////////////////////////////////////////////////////////////////////////////////
// IvfflatFileWriter
////////////////////////////////////////////////////////////////////////////////

RC IvfflatFileWriter::open(const char *filename) {
  filename_ = filename;
  file_.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!file_.is_open()) {
    LOG_ERROR("Failed to open file for writing: %s", filename);
    return RC::IOERR_OPEN;
  }
  return RC::SUCCESS;
}

RC IvfflatFileWriter::write_header(const IvfflatFileHeader &header) {
  file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
  if (!file_.good()) {
    LOG_ERROR("Failed to write header");
    return RC::IOERR_WRITE;
  }
  return RC::SUCCESS;
}

RC IvfflatFileWriter::write_centroids(const std::vector<std::vector<float>> &centroids) {
  uint32_t count = centroids.size();
  file_.write(reinterpret_cast<const char*>(&count), sizeof(count));
  
  for (const auto &centroid : centroids) {
    uint32_t dim = centroid.size();
    file_.write(reinterpret_cast<const char*>(centroid.data()), dim * sizeof(float));
  }
  
  if (!file_.good()) {
    LOG_ERROR("Failed to write centroids");
    return RC::IOERR_WRITE;
  }
  return RC::SUCCESS;
}

RC IvfflatFileWriter::write_bucket(size_t bucket_idx, 
    const std::vector<std::pair<std::vector<float>, RID>> &vectors) {
  
  uint32_t bucket_size = vectors.size();
  file_.write(reinterpret_cast<const char*>(&bucket_size), sizeof(bucket_size));
  
  for (const auto &[vec, rid] : vectors) {
    // 写入向量
    file_.write(reinterpret_cast<const char*>(vec.data()), vec.size() * sizeof(float));
    // 写入 RID
    file_.write(reinterpret_cast<const char*>(&rid), sizeof(RID));
  }
  
  if (!file_.good()) {
    LOG_ERROR("Failed to write bucket %zu", bucket_idx);
    return RC::IOERR_WRITE;
  }
  return RC::SUCCESS;
}

RC IvfflatFileWriter::close() {
  if (file_.is_open()) {
    file_.close();
  }
  return RC::SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// IvfflatFileReader
////////////////////////////////////////////////////////////////////////////////

RC IvfflatFileReader::open(const char *filename) {
  filename_ = filename;
  file_.open(filename, std::ios::binary | std::ios::in);
  if (!file_.is_open()) {
    LOG_ERROR("Failed to open file for reading: %s", filename);
    return RC::IOERR_OPEN;
  }
  return RC::SUCCESS;
}

RC IvfflatFileReader::read_header(IvfflatFileHeader &header) {
  file_.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!file_.good()) {
    LOG_ERROR("Failed to read header");
    return RC::IOERR_READ;
  }
  
  if (!header.validate()) {
    LOG_ERROR("Invalid file format");
    return RC::INVALID_ARGUMENT;
  }
  
  centroids_offset_ = file_.tellg();  // 记录质心起始位置
  return RC::SUCCESS;
}

RC IvfflatFileReader::read_centroids(std::vector<std::vector<float>> &centroids, uint32_t dimension) {
  file_.seekg(centroids_offset_);
  
  uint32_t count;
  file_.read(reinterpret_cast<char*>(&count), sizeof(count));
  
  centroids.resize(count);
  for (auto &centroid : centroids) {
    centroid.resize(dimension);
    file_.read(reinterpret_cast<char*>(centroid.data()), dimension * sizeof(float));
  }
  
  if (!file_.good()) {
    LOG_ERROR("Failed to read centroids");
    return RC::IOERR_READ;
  }
  
  buckets_offset_ = file_.tellg();  // 记录 bucket 起始位置
  return RC::SUCCESS;
}

RC IvfflatFileReader::read_bucket(size_t bucket_idx, 
    std::vector<std::pair<std::vector<float>, RID>> &vectors, uint32_t dimension) {
  
  file_.seekg(buckets_offset_);
  
  // 跳过前面的 bucket
  for (size_t i = 0; i < bucket_idx; ++i) {
    uint32_t bucket_size;
    file_.read(reinterpret_cast<char*>(&bucket_size), sizeof(bucket_size));
    
    // 跳过向量和 RID
    size_t skip_size = bucket_size * (dimension * sizeof(float) + sizeof(RID));
    file_.seekg(skip_size, std::ios::cur);
  }
  
  // 读取目标 bucket
  uint32_t bucket_size;
  file_.read(reinterpret_cast<char*>(&bucket_size), sizeof(bucket_size));
  
  vectors.clear();
  vectors.reserve(bucket_size);
  
  for (uint32_t i = 0; i < bucket_size; ++i) {
    std::vector<float> vec(dimension);
    file_.read(reinterpret_cast<char*>(vec.data()), dimension * sizeof(float));
    
    RID rid;
    file_.read(reinterpret_cast<char*>(&rid), sizeof(RID));
    
    vectors.emplace_back(std::move(vec), rid);
  }
  
  if (!file_.good()) {
    LOG_ERROR("Failed to read bucket %zu", bucket_idx);
    return RC::IOERR_READ;
  }
  
  return RC::SUCCESS;
}

RC IvfflatFileReader::close() {
  if (file_.is_open()) {
    file_.close();
  }
  return RC::SUCCESS;
}