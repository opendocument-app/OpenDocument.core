#include <odr/internal/zip/zip_file.hpp>

#include <odr/internal/zip/zip_archive.hpp>
#include <odr/internal/zip/zip_util.hpp>

namespace odr::internal::zip {

ZipFile::ZipFile(const std::shared_ptr<common::MemoryFile> &file)
    : m_zip{std::make_shared<util::Archive>(file)} {}

ZipFile::ZipFile(const std::shared_ptr<common::DiskFile> &file)
    : m_zip{std::make_shared<util::Archive>(file)} {}

std::shared_ptr<abstract::File> ZipFile::file() const noexcept {
  return m_zip->file();
}

FileType ZipFile::file_type() const noexcept { return FileType::zip; }

FileMeta ZipFile::file_meta() const noexcept {
  FileMeta meta;
  meta.type = file_type();
  return meta;
}

std::shared_ptr<abstract::Archive> ZipFile::archive() const {
  return std::make_shared<ZipArchive>(m_zip);
}

} // namespace odr::internal::zip
