#include <internal/pdf/pdf_file.h>
#include <poppler/PDFDoc.h>
#include <poppler/PDFDocFactory.h>

namespace odr::internal::pdf {

PdfFile::PdfFile(std::string path) {
  m_doc = PDFDocFactory().createPDFDoc(GooString(std::move(path)), nullptr,
                                       nullptr);
}

std::shared_ptr<abstract::File> PdfFile::file() const noexcept {
  return nullptr; // TODO
}

FileType PdfFile::file_type() const noexcept {
  return FileType::portable_document_format;
}

FileCategory PdfFile::file_category() const noexcept {
  return FileCategory::document;
}

FileMeta PdfFile::file_meta() const noexcept {
  return {}; // TODO
}

PDFDoc *PdfFile::doc() const { return m_doc; }

} // namespace odr::internal::pdf
