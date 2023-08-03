#include <internal/json/json_file.h>

#include <internal/json/json_util.h>

#include <iostream>

namespace odr::internal::json {

JsonFile::JsonFile(std::shared_ptr<internal::text::TextFile> file)
    : m_file{std::move(file)} {
  // TODO use text file?
  check_json_file(*m_file->file()->stream());
}

std::shared_ptr<internal::abstract::File> JsonFile::file() const noexcept {
  return m_file->file();
}

FileType JsonFile::file_type() const noexcept {
  return FileType::javascript_object_notation;
}

FileMeta JsonFile::file_meta() const noexcept {
  return {FileType::javascript_object_notation, false, {}};
}

} // namespace odr::internal::json
