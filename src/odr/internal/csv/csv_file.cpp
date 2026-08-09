#include <odr/internal/csv/csv_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/csv/csv_util.hpp>

#include <utility>

namespace odr::internal::csv {

CsvFile::CsvFile(std::shared_ptr<text::TextFile> file)
    : m_file{std::move(file)} {
  const Probe probe = csv::probe(*m_file->file(), m_file->encoding());
  if (!probe.is_csv) {
    throw NoCsvFile();
  }
  m_dialect = probe.dialect;
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

bool CsvFile::is_decodable() const noexcept { return false; }

TextEncoding CsvFile::encoding() const noexcept { return m_file->encoding(); }

Dialect CsvFile::dialect() const noexcept { return m_dialect; }

} // namespace odr::internal::csv
