#ifndef ODR_ZIP_ARCHIVE_H
#define ODR_ZIP_ARCHIVE_H

#include <common/archive.h>
#include <common/path.h>
#include <miniz.h>
#include <vector>

namespace odr::zip {
class ZipFile;

class ZipArchive final : public common::DefaultArchive {
public:
  ZipArchive();
  explicit ZipArchive(std::shared_ptr<const ZipFile> file);

  void save(const common::Path &path) const final;

private:
  std::shared_ptr<const ZipFile> m_file;
};

class ZipArchiveEntry final : public common::DefaultArchiveEntry {
public:
  ZipArchiveEntry(common::Path path, std::shared_ptr<common::File> file);

  [[nodiscard]] std::uint8_t compression_level() const;
  void compression_level(std::uint8_t);

private:
  std::uint8_t m_compression_level{6};
};

} // namespace odr::zip

#endif // ODR_ZIP_ARCHIVE_H
