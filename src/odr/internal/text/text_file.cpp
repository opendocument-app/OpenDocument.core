#include <odr/internal/text/text_file.hpp>

#include <odr/internal/text/text_util.hpp>

#include <iostream>

namespace odr::internal::text {

TextFile::TextFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)}, m_charset{guess_charset(*m_file->stream())} {}

TextFile::TextFile(std::shared_ptr<abstract::File> file, std::string charset)
    : m_file{std::move(file)}, m_charset{std::move(charset)} {}

std::shared_ptr<abstract::File> TextFile::file() const noexcept {
  return m_file;
}

FileType TextFile::file_type() const noexcept { return FileType::text_file; }

FileMeta TextFile::file_meta() const noexcept {
  return {FileType::text_file, false, {}};
}

DecoderEngine TextFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

} // namespace odr::internal::text
