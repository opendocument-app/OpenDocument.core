#ifndef ODR_COMMON_HTML_WRITER_H
#define ODR_COMMON_HTML_WRITER_H

#include <access/Stream.h>
#include <vector>
#include <string>

namespace odr {
namespace common {

class NestedWriter {
public:
  virtual ~NestedWriter() = default;

private:
  std::unique_ptr<Sink> sink_;
  NestedWriter &parent_;
};

class HtmlWriter final {
public:
  void startElement(const std::string &name);

  // TODO attribute writer

  void endElement();
  void endElement(const std::string &name);

private:
  std::unique_ptr<Sink> sink_;
  std::vector<std::string> stack_;
};

}
}

#endif // ODR_COMMON_HTML_WRITER_H
