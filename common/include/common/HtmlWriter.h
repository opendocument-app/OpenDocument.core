#ifndef ODR_COMMON_HTML_WRITER_H
#define ODR_COMMON_HTML_WRITER_H

#include <access/Stream.h>
#include <vector>
#include <string>
#include <memory>

namespace odr {
namespace common {

class HtmlAttributeValueWriter;
class HtmlCommentWriter;
class HtmlTextWriter;
class HtmlElementWriter;
class CssWriter;

class HtmlWriter {
public:
  virtual ~HtmlWriter() = default;

  void addDoctype();

  void startElement(const std::string &name);

  void addAttribute(const std::string &name, const std::string &value);

  void addComment(const std::string &text);

  void addText(const std::string &text);

  void endElement(const std::string &name);
};

class HtmlDocumentWriter final {
public:
  explicit HtmlDocumentWriter(std::unique_ptr<access::Sink> sink);
  ~HtmlDocumentWriter();

  HtmlElementWriter root();

private:
  std::unique_ptr<access::Sink> sink_;
  std::vector<std::string> stack_;

  void addDoctype_();
};

class HtmlElementWriter final {
public:
  explicit HtmlElementWriter(std::unique_ptr<access::Sink> sink);
  ~HtmlElementWriter();

  void addAttribute(const std::string &name, const std::string &value);
  HtmlAttributeValueWriter addAttribute(const std::string &name);

  void addComment(const std::string &text);
  HtmlCommentWriter addComment();

  void addText(const std::string &text);
  HtmlTextWriter addText();

private:

};

}
}

#endif // ODR_COMMON_HTML_WRITER_H
