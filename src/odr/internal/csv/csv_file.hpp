#pragma once

#include <odr/file.hpp>

#include <odr/internal/text/text_file.hpp>

#include <memory>

namespace odr::internal::csv {

class CsvFile final : public abstract::TextFile {
public:
  explicit CsvFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

private:
  std::shared_ptr<text::TextFile> m_file;
  std::string m_charset;
};

} // namespace odr::internal::csv
