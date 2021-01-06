#include <common/path.h>
#include <zip/zip_archive.h>
#include <zip/zip_file.h>

namespace odr::zip {

ZipArchive::ZipArchive() = default;

ZipArchive::ZipArchive(std::shared_ptr<const ZipFile> file)
    : m_file{std::move(file)} {
  auto num_files = mz_zip_reader_get_num_files(m_file->impl());
  for (std::uint32_t i = 0; i < num_files; ++i) {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(m_file->impl(), i, &stat);

    if (stat.m_is_directory) {
      DefaultArchive::insert_directory(DefaultArchive::end(), stat.m_filename);
    } else {
      // TODO compression_level
      DefaultArchive::insert_file(DefaultArchive::end(), stat.m_filename,
                                  nullptr); // TODO file
    }
  }
}

void ZipArchive::save(const common::Path &path) const {
  // TODO
}

ZipArchiveEntry::ZipArchiveEntry(common::Path path,
                                 std::shared_ptr<common::File> file)
    : DefaultArchiveEntry(std::move(path), std::move(file)) {}

std::uint8_t ZipArchiveEntry::compression_level() const {
  return m_compression_level;
}

void ZipArchiveEntry::compression_level(const std::uint8_t compression_level) {
  m_compression_level = compression_level;
}

} // namespace odr::zip
