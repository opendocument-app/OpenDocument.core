#ifndef ODR_COMMON_HTML_WRITER_H
#define ODR_COMMON_HTML_WRITER_H

#include <access/Stream.h>
#include <vector>
#include <string>
#include <memory>

namespace odr {
namespace common {

class HtmlHeadWriter;
class CssWriter;
class ScriptWriter;
class HtmlElementWriter;

class HtmlWriter final {
public:
  HtmlHeadWriter head(const std::string &title);

private:
};

class HtmlHeadWriter final {
public:
  HtmlHeadWriter meta(const std::string &charset);
  HtmlHeadWriter meta(const std::string &name, const std::string &content);

  CssWriter style();
  HtmlHeadWriter link();

  ScriptWriter script();
  HtmlHeadWriter script(const std::string &href);

  HtmlElementWriter body();

private:
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
