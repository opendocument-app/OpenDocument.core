#pragma once

#include <odr/file.hpp>
#include <odr/internal/abstract/file.hpp>

#include <memory>

namespace odr::internal::abstract {
class Image;
} // namespace odr::internal::abstract

namespace odr::internal::svm {

class SvmFile final : public abstract::ImageFile {
public:
  explicit SvmFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Image> image() const override;

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal::svm
