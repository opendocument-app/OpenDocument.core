#include <zip/zip_archive.h>

namespace odr::zip {

ZipArchive::ZipArchive() = default;

ZipArchive::ZipArchive(std::shared_ptr<const ZipFile> file)
    : m_file{std::move(file)} {}

} // namespace odr::zip
