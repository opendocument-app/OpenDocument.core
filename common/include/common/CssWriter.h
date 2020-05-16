#ifndef ODR_COMMON_CSS_WRITER_H
#define ODR_COMMON_CSS_WRITER_H

#include <access/Stream.h>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

namespace odr {
namespace common {

class CssDeclarationWriter;

class CssWriter final {
public:
  explicit CssWriter(std::ostream &output);

  CssDeclarationWriter selector(const std::string &selector);

private:
  std::ostream &output_;
};

class CssDeclarationWriter final {
public:
  explicit CssDeclarationWriter(std::ostream &output);
  ~CssDeclarationWriter();

  CssDeclarationWriter &property(const std::string &property,
                                 const std::string &value);

private:
  std::ostream &output_;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_CSS_WRITER_H
