#pragma once

#include <odr/file.hpp>

#include <odr/internal/csv/csv_util.hpp>
#include <odr/internal/text/text_file.hpp>

#include <memory>

namespace odr::internal::csv {

class CsvFile final : public abstract::TextFile {
public:
  explicit CsvFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] TextEncoding encoding() const noexcept override;

  /// The dialect detection resolved.
  [[nodiscard]] Dialect dialect() const noexcept;

private:
  std::shared_ptr<text::TextFile> m_file;
  Dialect m_dialect;
};

} // namespace odr::internal::csv
