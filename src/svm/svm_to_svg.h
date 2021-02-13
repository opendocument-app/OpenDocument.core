#ifndef ODR_SVM_TO_SVG_H
#define ODR_SVM_TO_SVG_H

#include <iostream>
#include <memory>

namespace odr::svm {
class SvmFile;

namespace Translator {
void svg(const SvmFile &file, std::ostream &out);
}
} // namespace odr::svm

#endif // ODR_SVM_TO_SVG_H
