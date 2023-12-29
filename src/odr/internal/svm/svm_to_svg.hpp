#ifndef ODR_INTERNAL_SVM_TO_SVG_HPP
#define ODR_INTERNAL_SVM_TO_SVG_HPP

#include <iostream>
#include <memory>

namespace odr::internal::svm {
class SvmFile;

namespace Translator {
void svg(const SvmFile &file, std::ostream &out);
}
} // namespace odr::internal::svm

#endif // ODR_INTERNAL_SVM_TO_SVG_HPP
