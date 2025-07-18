#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr::internal {

class ImageFile : public abstract::ImageFile {
public:
  ImageFile(std::shared_ptr<abstract::File> file, FileType file_type);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::Image> image() const final;

private:
  std::shared_ptr<abstract::File> m_file;
  FileType m_file_type;
};

} // namespace odr::internal
