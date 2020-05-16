#include <common/CssWriter.h>

namespace odr {
namespace common {

CssWriter::CssWriter(std::ostream &output)
    : output_(output) {}

CssDeclarationWriter CssWriter::selector(const std::string &selector) {
  output_.write(selector.data(), selector.size());
  return CssDeclarationWriter(output_);
}

CssDeclarationWriter::CssDeclarationWriter(std::ostream &output)
    : output_(output) {
  output_ << "{";
}

CssDeclarationWriter::~CssDeclarationWriter() {
  output_ << "}";
}

CssDeclarationWriter &
CssDeclarationWriter::property(const std::string &property,
                               const std::string &value) {
  output_ << property << ":" << value;
  return *this;
}

} // namespace common
} // namespace odr
