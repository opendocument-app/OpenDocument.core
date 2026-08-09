#include <odr/internal/text/text_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/encoding/detect.hpp>
#include <odr/internal/encoding/transcode.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <utility>

namespace odr::internal::text {

TextFile::TextFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  const std::unique_ptr<std::istream> in = m_file->stream();
  // nothing here rejects: text is the fallback for bytes nothing else claims,
  // so a viewer handed a random binary shows junk rather than an error, and
  // bytes we cannot name are `TextEncoding::unknown`
  m_encoding = encoding::detect(encoding::read_probe(*in));
}

TextFile::TextFile(std::shared_ptr<abstract::File> file,
                   const TextEncoding encoding)
    : m_file{std::move(file)}, m_encoding{encoding} {}

std::shared_ptr<abstract::File> TextFile::file() const noexcept {
  return m_file;
}

FileType TextFile::file_type() const noexcept { return FileType::text_file; }

std::string_view TextFile::mimetype() const noexcept { return "text/plain"; }

FileMeta TextFile::file_meta() const noexcept {
  FileMeta result;
  result.type = file_type();
  result.mimetype = mimetype();
  return result;
}

bool TextFile::is_decodable() const noexcept { return false; }

TextEncoding TextFile::encoding() const noexcept { return m_encoding; }

std::string TextFile::text() const {
  if (!text_encoding_is_decodable(m_encoding)) {
    throw UnsupportedTextEncoding(m_encoding);
  }
  const std::unique_ptr<std::istream> in = m_file->stream();
  return encoding::to_utf8(util::stream::read(*in), m_encoding);
}

} // namespace odr::internal::text
