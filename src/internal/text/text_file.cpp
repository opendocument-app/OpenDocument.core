#include <internal/text/text_file.h>
#include <uchardet.h>

namespace odr::internal::text {

TextFile::TextFile(std::shared_ptr<internal::abstract::File> file)
    : m_file{std::move(file)} {}

std::shared_ptr<internal::abstract::File> TextFile::file() const noexcept {
  return m_file;
}

FileType TextFile::file_type() const noexcept { return FileType::text_file; }

FileMeta TextFile::file_meta() const noexcept {
  return {}; // TODO
}

} // namespace odr::internal::text
