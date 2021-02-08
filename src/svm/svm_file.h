#ifndef ODR_SVM_TO_SVG_H
#define ODR_SVM_TO_SVG_H

#include <abstract/file.h>
#include <memory>

namespace odr::svm {

class SvmFile : public abstract::ImageFile {
public:
  explicit SvmFile(std::shared_ptr<abstract::File> file);

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::svm

#endif // ODR_SVM_TO_SVG_H
