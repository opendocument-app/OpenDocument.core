#ifndef ODR_SVM_SVM2SVG_H
#define ODR_SVM_SVM2SVG_H

#include <iostream>
#include <memory>

namespace odr {
namespace svm {

class NoSvmFileException : public std::exception {
public:
  const char *what() const noexcept override { return "not a svm file"; }
};

class MalformedSvmFileException : public std::exception {
public:
  const char *what() const noexcept override { return "malformed svm file"; }
};

namespace Translator {
void svg(std::istream &in, std::ostream &out);
}

} // namespace svm
} // namespace odr

#endif // ODR_SVM_SVM2SVG_H
