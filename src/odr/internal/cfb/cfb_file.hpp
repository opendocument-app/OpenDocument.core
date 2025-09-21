#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr {
enum class FileType;
struct FileMeta;
} // namespace odr

namespace odr::internal {
class MemoryFile;
} // namespace odr::internal

namespace odr::internal::cfb::util {
class Archive;
}

namespace odr::internal::cfb {

class CfbFile final : public abstract::ArchiveFile {
public:
  explicit CfbFile(const std::shared_ptr<MemoryFile> &file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Archive> archive() const override;

private:
  std::shared_ptr<util::Archive> m_cfb;
};

} // namespace odr::internal::cfb
