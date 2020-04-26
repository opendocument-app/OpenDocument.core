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
  explicit CssWriter(std::unique_ptr<std::ostream> output);
  CssWriter(std::unique_ptr<std::ostream> output,
            std::unique_ptr<std::ostream> &owner);
  ~CssWriter();

  CssDeclarationWriter selector(const std::string &selector);

private:
  std::unique_ptr<std::ostream> output_;
  std::unique_ptr<std::ostream> &owner_;
};

class CssDeclarationWriter final {
public:
  explicit CssDeclarationWriter(std::unique_ptr<std::ostream> output);
  CssDeclarationWriter(std::unique_ptr<std::ostream> output,
                       std::unique_ptr<std::ostream> &owner);
  ~CssDeclarationWriter();

  CssDeclarationWriter &declaration(const std::string &property,
                                    const std::string &value);

private:
  std::unique_ptr<std::ostream> output_;
  std::unique_ptr<std::ostream> &owner_;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_CSS_WRITER_H
