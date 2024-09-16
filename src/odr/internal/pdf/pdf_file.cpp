#include <odr/internal/pdf/pdf_file.hpp>

namespace odr::internal {

PdfFile::PdfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {}

FileCategory PdfFile::file_category() const noexcept {
  return FileCategory::document;
}

std::shared_ptr<abstract::File> PdfFile::file() const noexcept {
  return m_file;
}

FileType PdfFile::file_type() const noexcept {
  return FileType::portable_document_format;
}

FileMeta PdfFile::file_meta() const noexcept { return {}; }

DecoderEngine PdfFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

} // namespace odr::internal
