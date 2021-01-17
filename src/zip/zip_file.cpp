#include <common/path.h>
#include <miniz.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <zip/zip_archive.h>
#include <zip/zip_file.h>

namespace odr::zip {

ZipFile::ZipFile(const common::Path &path)
    : ZipFile(std::make_shared<common::DiscFile>(path)) {}

ZipFile::ZipFile(const std::shared_ptr<common::DiscFile> &file) : m_file{file} {
  memset(&m_zip, 0, sizeof(m_zip));
  const bool state =
      mz_zip_reader_init_file(&m_zip, file->path().string().data(),
                              MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
  if (!state) {
    throw NoZipFile();
  }
}

ZipFile::ZipFile(std::shared_ptr<common::MemoryFile> file)
    : m_file{std::move(file)} {
  memset(&m_zip, 0, sizeof(m_zip));
  const bool state = mz_zip_reader_init_mem(
      &m_zip, file->content().data(), file->content().size(),
      MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
  if (!state) {
    throw NoZipFile();
  }
}

ZipFile::~ZipFile() { mz_zip_reader_end(&m_zip); }

FileType ZipFile::file_type() const noexcept { return FileType::ZIP; }

FileCategory ZipFile::file_category() const noexcept {
  return FileCategory::ARCHIVE;
}

FileMeta ZipFile::file_meta() const noexcept {
  return {}; // TODO
}

FileLocation ZipFile::file_location() const noexcept {
  return m_file->file_location();
}

std::size_t ZipFile::size() const { return m_file->size(); }

std::unique_ptr<std::istream> ZipFile::data() const { return m_file->data(); }

mz_zip_archive *ZipFile::impl() const { return &m_zip; }

std::shared_ptr<ZipArchive> ZipFile::archive() const {
  return std::make_shared<ZipArchive>(shared_from_this());
}

} // namespace odr::zip
