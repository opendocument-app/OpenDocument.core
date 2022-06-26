#ifndef ODR_INTERNAL_PDF_FILE_H
#define ODR_INTERNAL_PDF_FILE_H

#include <internal/abstract/file.h>
#include <memory>

class PDFDoc;

namespace odr::internal::pdf {

class PdfFile final : public abstract::DecodedFile {
public:
  explicit PdfFile(std::string path);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileCategory file_category() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;

  [[nodiscard]] PDFDoc *doc() const;

private:
  PDFDoc *m_doc{nullptr};
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_FILE_H
