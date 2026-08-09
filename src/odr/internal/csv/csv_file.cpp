#include <odr/internal/csv/csv_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/csv/csv_document.hpp>
#include <odr/internal/csv/csv_util.hpp>

#include <stdexcept>
#include <utility>

namespace odr::internal::csv {

namespace {

/// An incoherent dialect is a caller mistake, not bad input.
void check(const Dialect dialect) {
  if (dialect.separator == dialect.quote) {
    throw std::invalid_argument("csv separator equals its quote");
  }
  if (dialect.separator == '\n' || dialect.separator == '\r') {
    throw std::invalid_argument("csv separator is a line break");
  }
}

} // namespace

CsvFile::CsvFile(std::shared_ptr<text::TextFile> file)
    : m_file{std::move(file)} {
  const Probe probe = csv::probe(*m_file->file(), m_file->encoding());
  if (!probe.is_csv) {
    throw NoCsvFile();
  }
  m_dialect = probe.dialect;
  m_separator_directive = probe.separator_directive;
}

CsvFile::CsvFile(std::shared_ptr<abstract::File> file,
                 const CsvOptions &options) {
  m_file =
      options.encoding.has_value()
          ? std::make_shared<text::TextFile>(std::move(file), *options.encoding)
          : std::make_shared<text::TextFile>(std::move(file));

  m_dialect.quote = options.quote.value_or(m_dialect.quote);

  if (options.separator.has_value()) {
    m_dialect.separator = *options.separator;
    check(m_dialect);
    return;
  }

  const Probe probe =
      csv::probe(*m_file->file(), m_file->encoding(), m_dialect.quote);
  if (!probe.is_csv) {
    throw NoCsvFile();
  }
  m_dialect.separator = probe.dialect.separator;
  m_separator_directive = probe.separator_directive;
  check(m_dialect);
}

std::shared_ptr<abstract::File> CsvFile::file() const noexcept {
  return m_file->file();
}

FileType CsvFile::file_type() const noexcept {
  return FileType::comma_separated_values;
}

std::string_view CsvFile::mimetype() const noexcept { return "text/csv"; }

FileMeta CsvFile::file_meta() const noexcept {
  FileMeta result;
  result.type = file_type();
  result.mimetype = mimetype();
  return result;
}

bool CsvFile::is_decodable() const noexcept {
  return text_encoding_is_decodable(encoding());
}

std::shared_ptr<abstract::Document> CsvFile::document() const {
  if (!is_decodable()) {
    throw UnsupportedTextEncoding(encoding());
  }
  return std::make_shared<CsvDocument>(*m_file->file(), encoding(), m_dialect,
                                       m_separator_directive);
}

TextEncoding CsvFile::encoding() const noexcept { return m_file->encoding(); }

CsvOptions CsvFile::options() const {
  return {.encoding = encoding(),
          .separator = m_dialect.separator,
          .quote = m_dialect.quote};
}

std::shared_ptr<abstract::CsvFile>
CsvFile::with_options(const CsvOptions &options) const {
  return std::static_pointer_cast<abstract::CsvFile>(
      std::make_shared<CsvFile>(m_file->file(), options));
}

Dialect CsvFile::dialect() const noexcept { return m_dialect; }

bool CsvFile::separator_directive() const noexcept {
  return m_separator_directive;
}

} // namespace odr::internal::csv
