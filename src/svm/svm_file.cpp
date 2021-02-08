#include <svm/svm_file.h>

namespace odr::svm {

SvmFile::SvmFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  // TODO check magic
}

} // namespace odr::svm
