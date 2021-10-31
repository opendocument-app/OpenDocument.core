#ifndef ODR_INTERNAL_TEXT_FILE_H
#define ODR_INTERNAL_TEXT_FILE_H

#include <internal/abstract/file.h>
#include <memory>
#include <odr/file.h>

namespace odr::internal::text {

class TextFile final : public abstract::TextFile {
public:
  explicit TextFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal::text

#endif // ODR_INTERNAL_TEXT_FILE_H
