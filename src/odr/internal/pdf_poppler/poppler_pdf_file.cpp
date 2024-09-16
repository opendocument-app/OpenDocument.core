#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <odr/internal/common/path.hpp>

#include <poppler/PDFDocFactory.h>
#include <poppler/goo/GooString.h>

namespace odr::internal::poppler_pdf {

PopplerPdfFile::PopplerPdfFile(std::shared_ptr<common::DiskFile> file)
    : m_file{std::move(file)} {
  GooString file_path(file->disk_path()->string().c_str());
  m_pdf_doc = std::unique_ptr<PDFDoc>(PDFDocFactory().createPDFDoc(file_path));
}

FileCategory PopplerPdfFile::file_category() const noexcept {
  return FileCategory::document;
}

std::shared_ptr<abstract::File> PopplerPdfFile::file() const noexcept {
  return m_file;
}

FileType PopplerPdfFile::file_type() const noexcept {
  return FileType::portable_document_format;
}

FileMeta PopplerPdfFile::file_meta() const noexcept { return {}; }

DecoderEngine PopplerPdfFile::decoder_engine() const noexcept {
  return DecoderEngine::poppler;
}

const PDFDoc &PopplerPdfFile::pdf_doc() const { return *m_pdf_doc; }

} // namespace odr::internal::poppler_pdf
