#include "files.h"

auto get_file_size(const char *filename) -> size_t {
  std::ifstream ifs(filename, std::ios::in | std::ios::binary);
  if (!ifs.good()) {
    return -1;
  }
  ifs.seekg(0, std::ios::end);
  size_t size = (size_t)ifs.tellg();
  return size;
}

auto read_file_to_buf(const char *filename, uint8_t *buf, size_t bsize)
    -> bool {
  if (filename == NULL || buf == NULL || bsize == 0) return false;
  std::ifstream ifs(filename, std::ios::binary | std::ios::in);
  if (!ifs.good()) {
    return false;
  }
  ifs.read(reinterpret_cast<char *>(buf), bsize);
  return !ifs.fail();
}

auto write_buf_to_file(const char *filename, const uint8_t *buf, size_t bsize,
                       long offset) -> bool {
  if (filename == NULL || buf == NULL || bsize == 0) return false;
  std::ofstream ofs(filename, std::ios::binary | std::ios::out);
  if (!ofs.good()) {
    return false;
  }
  ofs.seekp(offset, std::ios::beg);
  ofs.write(reinterpret_cast<const char *>(buf), bsize);
  return !ofs.fail();
}