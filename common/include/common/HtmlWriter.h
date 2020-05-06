#ifndef ODR_COMMON_HTML_WRITER_H
#define ODR_COMMON_HTML_WRITER_H

#include <access/Stream.h>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace odr {
namespace common {

class HtmlHeadWriter;
class HtmlElementWriter;
class HtmlElementBodyWriter;

class HtmlWriter final {
public:
  HtmlWriter(std::unique_ptr<std::ostream> output);
  HtmlWriter(std::unique_ptr<std::ostream> output,
             std::unique_ptr<std::ostream> &owner);
  ~HtmlWriter();

  HtmlHeadWriter head();

private:
  std::unique_ptr<std::ostream> output_;
  std::unique_ptr<std::ostream> &owner_;
};

class HtmlHeadWriter final {
public:
  HtmlHeadWriter(std::unique_ptr<std::ostream> output);
  HtmlHeadWriter(std::unique_ptr<std::ostream> output,
                 std::unique_ptr<std::ostream> &owner);
  ~HtmlHeadWriter();

  HtmlHeadWriter &meta(const std::string &charset);
  HtmlHeadWriter &meta(const std::string &name,
                       const std::string &content);

  void style();
  HtmlHeadWriter &link();
  void script();
  void script(const std::string &href);
  HtmlElementWriter body();

private:
  std::unique_ptr<std::ostream> output_;
  std::unique_ptr<std::ostream> &owner_;
};

class HtmlElementWriter final {
public:
  HtmlElementWriter(std::unique_ptr<std::ostream> output);
  HtmlElementWriter(std::unique_ptr<std::ostream> output,
                    std::unique_ptr<std::ostream> &owner);
  ~HtmlElementWriter();

  HtmlElementWriter &attribute();
  HtmlElementBodyWriter comment();
  HtmlElementBodyWriter text();
  HtmlElementBodyWriter element(const std::string &name);

private:
  std::unique_ptr<std::ostream> output_;
  std::unique_ptr<std::ostream> &owner_;
};

class HtmlElementBodyWriter final {
public:
  HtmlElementBodyWriter(std::unique_ptr<std::ostream> output);
  HtmlElementBodyWriter(std::unique_ptr<std::ostream> output,
                        std::unique_ptr<std::ostream> &owner);
  ~HtmlElementBodyWriter();

  HtmlElementBodyWriter &comment();
  HtmlElementBodyWriter &text();
  HtmlElementBodyWriter &element(const std::string &name);

private:
  std::unique_ptr<std::ostream> output_;
  std::unique_ptr<std::ostream> &owner_;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_HTML_WRITER_H
