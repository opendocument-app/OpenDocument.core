#ifndef ODR_INTERNAL_PDF_FILE_HPP
#define ODR_INTERNAL_PDF_FILE_HPP

#include <odr/internal/abstract/file.hpp>

namespace odr::internal::pdf {

class PdfFile : public abstract::DecodedFile {
public:
  explicit PdfFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileCategory file_category() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_FILE_HPP
