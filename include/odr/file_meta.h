#ifndef ODR_FILE_META_H
#define ODR_FILE_META_H

#include <cstdint>
#include <odr/file_type.h>
#include <string>
#include <vector>

namespace odr {

struct FileMeta {
  static FileType type_by_extension(const std::string &extension) noexcept;

  struct Entry {
    std::string name;
    std::uint32_t row_count{0};
    std::uint32_t column_count{0};
    std::string notes;
  };

  FileType type{FileType::UNKNOWN};
  bool encrypted{false};
  std::uint32_t entry_count{0};
  std::vector<Entry> entries;

  [[nodiscard]] std::string type_as_string() const noexcept;
};

} // namespace odr

#endif // ODR_FILE_META_H
