#ifndef ODR_INTERNAL_POPPLER_PDF_FILE_HPP
#define ODR_INTERNAL_POPPLER_PDF_FILE_HPP

#include <odr/internal/common/file.hpp>

class PDFDoc;

namespace odr::internal::poppler_pdf {

class PopplerPdfFile : public abstract::DecodedFile {
public:
  explicit PopplerPdfFile(std::shared_ptr<common::DiskFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileCategory file_category() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] const PDFDoc &pdf_doc() const;

private:
  std::shared_ptr<abstract::File> m_file;
  std::unique_ptr<PDFDoc> m_pdf_doc;
};

} // namespace odr::internal::poppler_pdf

#endif // ODR_INTERNAL_POPPLER_PDF_FILE_HPP
