#ifndef ODR_INTERNAL_CSV_FILE_H
#define ODR_INTERNAL_CSV_FILE_H

#include <memory>
#include <odr/file.h>
#include <odr/internal/text/text_file.h>

namespace odr::internal::csv {

class CsvFile final : public abstract::TextFile {
public:
  explicit CsvFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;

private:
  std::shared_ptr<text::TextFile> m_file;
  std::string m_charset;
};

} // namespace odr::internal::csv

#endif // ODR_INTERNAL_CSV_FILE_H
