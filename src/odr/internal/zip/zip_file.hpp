#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr {
enum class FileType;
struct FileMeta;
} // namespace odr

namespace odr::internal::zip {
namespace util {
class Archive;
}

class ZipFile final : public abstract::ArchiveFile {
public:
  explicit ZipFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;
  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Archive> archive() const override;

private:
  std::shared_ptr<util::Archive> m_zip;
};

} // namespace odr::internal::zip
