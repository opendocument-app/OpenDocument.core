#ifndef ODR_INTERNAL_JSON_FILE_H
#define ODR_INTERNAL_JSON_FILE_H

#include <internal/text/text_file.h>
#include <memory>
#include <odr/file.h>

namespace odr::internal::json {

class JsonFile final : public abstract::TextFile {
public:
  explicit JsonFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;

private:
  std::shared_ptr<text::TextFile> m_file;
  std::string m_charset;
};

} // namespace odr::internal::json

#endif // ODR_INTERNAL_JSON_FILE_H
