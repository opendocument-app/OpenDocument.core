#include <zip/zip_archive.h>

namespace odr::zip {

ZipArchive::ZipArchive() = default;

ZipArchive::ZipArchive(std::shared_ptr<const ZipFile> file)
    : m_file{std::move(file)} {}

ZipArchiveEntry::ZipArchiveEntry(std::shared_ptr<const ZipArchive> archive,
                                 mz_uint index)
    : m_archive{std::move(archive)}, m_index{index} {}

} // namespace odr::zip
