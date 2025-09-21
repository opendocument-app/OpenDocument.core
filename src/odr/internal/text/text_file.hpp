#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>

namespace odr::internal::text {

class TextFile final : public abstract::TextFile {
public:
  explicit TextFile(std::shared_ptr<abstract::File> file);
  TextFile(std::shared_ptr<abstract::File> file, std::string charset);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

private:
  std::shared_ptr<abstract::File> m_file;
  std::string m_charset;
};

} // namespace odr::internal::text
