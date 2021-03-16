#ifndef ODR_INTERNAL_SVM_FILE_H
#define ODR_INTERNAL_SVM_FILE_H

#include <internal/abstract/file.h>
#include <memory>

namespace odr::internal::svm {

class SvmFile final {
public:
  explicit SvmFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept;

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] FileMeta file_meta() const noexcept;

  [[nodiscard]] std::shared_ptr<abstract::Image> image() const;

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal::svm

#endif // ODR_INTERNAL_SVM_FILE_H
