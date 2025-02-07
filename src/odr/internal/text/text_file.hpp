#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>

namespace odr::internal::text {

class TextFile final : public abstract::TextFile {
public:
  explicit TextFile(std::shared_ptr<abstract::File> file);
  TextFile(std::shared_ptr<abstract::File> file, std::string charset);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

private:
  std::shared_ptr<abstract::File> m_file;
  std::string m_charset;
};

} // namespace odr::internal::text
