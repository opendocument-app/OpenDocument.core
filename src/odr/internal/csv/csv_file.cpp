#include <odr/internal/csv/csv_file.hpp>

#include <odr/internal/csv/csv_util.hpp>

#include <iostream>

namespace odr::internal::csv {

CsvFile::CsvFile(std::shared_ptr<text::TextFile> file)
    : m_file{std::move(file)} {
  // TODO use text file?
  check_csv_file(*m_file->file()->stream());
}

std::shared_ptr<abstract::File> CsvFile::file() const noexcept {
  return m_file->file();
}

FileType CsvFile::file_type() const noexcept {
  return FileType::comma_separated_values;
}

DecoderEngine CsvFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

std::string_view CsvFile::mimetype() const noexcept { return "text/csv"; }

FileMeta CsvFile::file_meta() const noexcept {
  return {FileType::comma_separated_values, "text/csv", false, std::nullopt};
}

bool CsvFile::is_decodable() const noexcept { return false; }

} // namespace odr::internal::csv
