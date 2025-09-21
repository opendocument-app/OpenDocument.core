#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr {
enum class FileType;
struct FileMeta;
} // namespace odr

namespace odr::internal {
class MemoryFile;
class DiskFile;
} // namespace odr::internal

namespace odr::internal::zip {
namespace util {
class Archive;
}

class ZipFile final : public abstract::ArchiveFile {
public:
  explicit ZipFile(const std::shared_ptr<MemoryFile> &file);
  explicit ZipFile(const std::shared_ptr<DiskFile> &file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Archive> archive() const override;

private:
  std::shared_ptr<util::Archive> m_zip;
};

} // namespace odr::internal::zip
