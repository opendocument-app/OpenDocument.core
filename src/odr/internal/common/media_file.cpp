#include <odr/internal/common/media_file.hpp>

#include <odr/odr.hpp>

namespace odr::internal {

MediaFile::MediaFile(std::shared_ptr<abstract::File> file,
                     const FileType file_type)
    : m_file{std::move(file)}, m_file_type{file_type} {}

std::shared_ptr<abstract::File> MediaFile::file() const noexcept {
  return m_file;
}

FileType MediaFile::file_type() const noexcept { return m_file_type; }

FileCategory MediaFile::file_category() const noexcept {
  return file_category_by_file_type(m_file_type);
}

FileMeta MediaFile::file_meta() const noexcept {
  FileMeta result;
  result.type = file_type();
  result.mimetype = mimetype();
  return result;
}

std::string_view MediaFile::mimetype() const noexcept {
  // not `mimetype_by_file_type` — that throws, and this is `noexcept`
  const std::span<const std::string_view> mimetypes =
      mimetypes_by_file_type(m_file_type);
  return mimetypes.empty() ? "" : mimetypes.front();
}

bool MediaFile::is_decodable() const noexcept { return false; }

} // namespace odr::internal
