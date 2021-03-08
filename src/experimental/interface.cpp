#include <experimental/interface.h>
#include <odr/experimental/file_meta.h>
#include <odr/file_meta.h>

namespace odr::experimental {

odr::FileMeta convert(const FileMeta &meta) {
  odr::FileMeta result;

  struct Entry {
    std::string name;
    std::uint32_t row_count{0};
    std::uint32_t column_count{0};
    std::string notes;
  };

  FileType type{FileType::UNKNOWN};
  bool confident{false};
  bool encrypted{false};
  std::uint32_t entry_count{0};
  std::vector<Entry> entries;

  result.type = meta.type;
  result.confident = true;
  result.encrypted = meta.password_encrypted;

  if (meta.document_meta) {
    result.entry_count = meta.document_meta->entry_count;

    for (auto &&entry : meta.document_meta->entries) {
      odr::FileMeta::Entry result_entry;

      if (entry.name) {
        result_entry.name = *entry.name;
      }
      if (entry.table_dimensions) {
        result_entry.row_count = entry.table_dimensions->rows;
        result_entry.column_count = entry.table_dimensions->columns;
      }
      if (entry.notes) {
        result_entry.notes = *entry.notes;
      }
    }
  }

  return result;
}

} // namespace odr::experimental
