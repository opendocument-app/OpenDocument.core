#pragma once

#include <odr/file.hpp>

#include <odr/internal/csv/csv_util.hpp>
#include <odr/internal/text/text_file.hpp>

#include <memory>

namespace odr::internal::csv {

class CsvFile final : public abstract::CsvFile {
public:
  /// Detects everything; the path `open_strategy` probes with.
  /// @throws NoCsvFile if it does not look like a csv.
  explicit CsvFile(std::shared_ptr<text::TextFile> file);

  /// Detects only what @p options leaves unset.
  /// @throws NoCsvFile if no separator was given and detection finds none.
  /// @throws std::invalid_argument if @p options is not coherent.
  CsvFile(std::shared_ptr<abstract::File> file, const CsvOptions &options);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

  [[nodiscard]] TextEncoding encoding() const noexcept override;

  [[nodiscard]] CsvOptions options() const override;
  [[nodiscard]] std::shared_ptr<abstract::CsvFile>
  with_options(const CsvOptions &options) const override;

  /// The dialect in use.
  [[nodiscard]] Dialect dialect() const noexcept;
  /// Whether the opening line is Excel's `sep=` directive rather than data.
  [[nodiscard]] bool separator_directive() const noexcept;

private:
  std::shared_ptr<text::TextFile> m_file;
  Dialect m_dialect;
  bool m_separator_directive{false};
};

} // namespace odr::internal::csv
