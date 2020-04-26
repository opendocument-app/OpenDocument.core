#include <common/CssWriter.h>

namespace odr {
namespace common {

CssWriter::CssWriter(std::unique_ptr<std::ostream> output)
    : output_(std::move(output)), owner_(output_) {}

CssWriter::CssWriter(std::unique_ptr<std::ostream> output,
                     std::unique_ptr<std::ostream> &owner)
    : output_(std::move(output)), owner_(owner) {}

CssWriter::~CssWriter() {
  if (output_ != owner_)
    owner_ = std::move(output_);
}

CssDeclarationWriter CssWriter::selector(const std::string &selector) {
  output_->write(selector.data(), selector.size());
  return CssDeclarationWriter(std::move(output_), output_);
}

CssDeclarationWriter::CssDeclarationWriter(std::unique_ptr<std::ostream> output)
    : output_(std::move(output)), owner_(output_) {
  output_->write("{", 1);
}

CssDeclarationWriter::CssDeclarationWriter(std::unique_ptr<std::ostream> output,
                                           std::unique_ptr<std::ostream> &owner)
    : output_(std::move(output)), owner_(owner) {
  output_->write("{", 1);
}

CssDeclarationWriter::~CssDeclarationWriter() {
  output_->write("}", 1);
  if (output_ != owner_)
    owner_ = std::move(output_);
}

CssDeclarationWriter &
CssDeclarationWriter::declaration(const std::string &property,
                                  const std::string &value) {
  output_->write(property.data(), property.size());
  output_->write(":", 1);
  output_->write(value.data(), value.size());
  return *this;
}

} // namespace common
} // namespace odr
