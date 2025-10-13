#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr::internal {

class ImageFile final : public abstract::ImageFile {
public:
  ImageFile(std::shared_ptr<abstract::File> file, FileType file_type);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Image> image() const override;

private:
  std::shared_ptr<abstract::File> m_file;
  FileType m_file_type;
};

} // namespace odr::internal
