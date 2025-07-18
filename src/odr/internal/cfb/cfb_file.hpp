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

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::Archive> archive() const final;

private:
  std::shared_ptr<util::Archive> m_cfb;
};

} // namespace odr::internal::cfb
