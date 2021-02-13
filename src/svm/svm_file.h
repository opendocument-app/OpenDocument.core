#ifndef ODR_SVM_FILE_H
#define ODR_SVM_FILE_H

#include <abstract/file.h>
#include <memory>

namespace odr::svm {

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

} // namespace odr::svm

#endif // ODR_SVM_FILE_H
