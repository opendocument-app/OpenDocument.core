#include <odr/internal/json/json_file.hpp>

#include <odr/internal/json/json_util.hpp>

#include <iostream>

namespace odr::internal::json {

JsonFile::JsonFile(std::shared_ptr<text::TextFile> file)
    : m_file{std::move(file)} {
  // TODO use text file?
  check_json_file(*m_file->file()->stream());
}

std::shared_ptr<abstract::File> JsonFile::file() const noexcept {
  return m_file->file();
}

FileType JsonFile::file_type() const noexcept {
  return FileType::javascript_object_notation;
}

FileMeta JsonFile::file_meta() const noexcept {
  return {FileType::javascript_object_notation, false, {}};
}

DecoderEngine JsonFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

bool JsonFile::is_decodable() const noexcept { return false; }

} // namespace odr::internal::json
