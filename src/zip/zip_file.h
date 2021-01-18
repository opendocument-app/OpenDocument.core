#ifndef ODR_ZIP_FILE_H
#define ODR_ZIP_FILE_H

#include <common/file.h>
#include <miniz.h>

namespace odr::common {
class Path;
}

namespace odr::zip {
class ZipArchive;

class ZipFile final : public abstract::File,
                      public std::enable_shared_from_this<ZipFile> {
public:
  explicit ZipFile(const std::shared_ptr<common::DiscFile> &file);
  explicit ZipFile(const std::shared_ptr<common::MemoryFile> &file);
  ~ZipFile() final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileCategory file_category() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] FileLocation file_location() const noexcept final;

  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] std::unique_ptr<std::istream> data() const final;

  [[nodiscard]] mz_zip_archive *impl() const;

  [[nodiscard]] std::shared_ptr<ZipArchive> archive() const;

private:
  mutable mz_zip_archive m_zip{};
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::zip

#endif // ODR_ZIP_FILE_H
