#pragma once

#include <iostream>
#include <memory>

namespace odr::internal::svm {
class SvmFile;

namespace Translator {
void svg(const SvmFile &file, std::ostream &out);
}
} // namespace odr::internal::svm
