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
  explicit HtmlWriter(std::ostream &output);
  ~HtmlWriter();

  HtmlHeadWriter head(const std::string &title) const;
  HtmlElementWriter body() const;

private:
  std::ostream &output_;
};

class HtmlHeadWriter final {
public:
  HtmlHeadWriter(const std::string &title, std::ostream &output);
  ~HtmlHeadWriter();

  const HtmlHeadWriter &meta(const std::string &charset) const;
  const HtmlHeadWriter &meta(const std::string &name, const std::string &content) const;

  void style() const;
  const HtmlHeadWriter &link() const;
  void script() const;
  void script(const std::string &href) const;

private:
  std::ostream &output_;
};

class HtmlElementWriter final {
public:
  HtmlElementWriter(std::string name, std::ostream &output);
  ~HtmlElementWriter();

  const HtmlElementWriter &attribute(const std::string &name, const std::string &value) const;
  HtmlElementBodyWriter comment(const std::string &content) const;
  HtmlElementBodyWriter text(const std::string &content) const;
  HtmlElementWriter element(const std::string &name) const;

private:
  std::string name_;
  std::ostream &output_;
};

class HtmlElementBodyWriter final {
public:
  explicit HtmlElementBodyWriter(std::ostream &output);

  const HtmlElementBodyWriter &comment(const std::string &content) const;
  const HtmlElementBodyWriter &text(const std::string &content) const;
  HtmlElementWriter element(const std::string &name) const;

private:
  std::ostream &output_;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_HTML_WRITER_H
