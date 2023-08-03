#ifndef ODR_INTERNAL_SVM_FILE_H
#define ODR_INTERNAL_SVM_FILE_H

#include <memory>
#include <odr/file.hpp>
#include <odr/internal/abstract/file.hpp>

namespace odr::internal::abstract {
class Image;
} // namespace odr::internal::abstract

namespace odr::internal::svm {

class SvmFile final : public abstract::ImageFile {
public:
  explicit SvmFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::Image> image() const final;

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal::svm

#endif // ODR_INTERNAL_SVM_FILE_H
