#ifndef ODR_INTERNAL_ZIP_FILE_HPP
#define ODR_INTERNAL_ZIP_FILE_HPP

#include <odr/internal/abstract/file.hpp>

namespace odr {
enum class FileType;
struct FileMeta;
} // namespace odr

namespace odr::internal::common {
class MemoryFile;
class DiskFile;
} // namespace odr::internal::common

namespace odr::internal::zip {
namespace util {
class Archive;
}

class ZipFile final : public abstract::ArchiveFile {
public:
  explicit ZipFile(const std::shared_ptr<common::MemoryFile> &file);
  explicit ZipFile(const std::shared_ptr<common::DiskFile> &file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::Archive> archive() const final;

private:
  std::shared_ptr<util::Archive> m_zip;
};

} // namespace odr::internal::zip

#endif // ODR_INTERNAL_ZIP_FILE_HPP
