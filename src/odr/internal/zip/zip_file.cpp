#include <odr/internal/zip/zip_file.hpp>

#include <odr/internal/zip/zip_archive.hpp>
#include <odr/internal/zip/zip_util.hpp>

namespace odr::internal::zip {

ZipFile::ZipFile(const std::shared_ptr<MemoryFile> &file)
    : m_zip{std::make_shared<util::Archive>(file)} {}

ZipFile::ZipFile(const std::shared_ptr<DiskFile> &file)
    : m_zip{std::make_shared<util::Archive>(file)} {}

std::shared_ptr<abstract::File> ZipFile::file() const noexcept {
  return m_zip->file();
}

DecoderEngine ZipFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

FileType ZipFile::file_type() const noexcept { return FileType::zip; }

std::string_view ZipFile::mimetype() const noexcept {
  return "application/zip";
}

FileMeta ZipFile::file_meta() const noexcept {
  return {file_type(), mimetype(), false, std::nullopt};
}

bool ZipFile::is_decodable() const noexcept { return true; }

std::shared_ptr<abstract::Archive> ZipFile::archive() const {
  return std::make_shared<ZipArchive>(m_zip);
}

} // namespace odr::internal::zip
