#include <common/HtmlWriter.h>

#include <utility>

namespace odr {
namespace common {

HtmlWriter::HtmlWriter(std::ostream &output) : output_{output} {
  output_ << "<!DOCTYPE html>\n";
  output_ << "<html>";
}

HtmlWriter::~HtmlWriter() { output_ << "</html>"; }

HtmlHeadWriter HtmlWriter::head(const std::string &title) const {
  return HtmlHeadWriter(title, output_);
}

HtmlElementWriter HtmlWriter::body() const {
  return HtmlElementWriter("body", output_);
}

HtmlHeadWriter::HtmlHeadWriter(const std::string &title, std::ostream &output)
    : output_{output} {
  output_ << "<head>";
  output_ << "<title>";
  output_ << "title";
  output_ << "</title>";
}

HtmlHeadWriter::~HtmlHeadWriter() { output_ << "</head>"; }

const HtmlHeadWriter &HtmlHeadWriter::meta(const std::string &charset) const {
  output_ << "<meta charset=\"" << charset << "\">";
  return *this;
}

const HtmlHeadWriter &HtmlHeadWriter::meta(const std::string &name,
                                     const std::string &content) const {
  output_ << "<meta name=\"" << name << "\" content=\"" << content << "\">";
  return *this;
}

HtmlElementWriter::HtmlElementWriter(std::string name, std::ostream &output)
    : name_{std::move(name)}, output_{output} {
  output_ << "<" << name_ << ">";
}

HtmlElementWriter::~HtmlElementWriter() { output_ << "</" << name_ << ">"; }

const HtmlElementWriter &HtmlElementWriter::attribute(const std::string &name, const std::string &value) const {
  output_ << " " << name << "\"" << value << "\"";
  return *this;
}

HtmlElementBodyWriter HtmlElementWriter::comment(const std::string &content) const {
  return HtmlElementBodyWriter(output_).comment(content);
}

HtmlElementBodyWriter HtmlElementWriter::text(const std::string &content) const {
  return HtmlElementBodyWriter(output_).text(content);
}

HtmlElementWriter HtmlElementWriter::element(const std::string &name) const {
  return HtmlElementWriter(name, output_);
}

HtmlElementBodyWriter::HtmlElementBodyWriter(std::ostream &output)
    : output_{output} {}

const HtmlElementBodyWriter &HtmlElementBodyWriter::comment(const std::string &content) const {
  output_ << "<!-- " << content << " -->";
  return *this;
}

const HtmlElementBodyWriter &HtmlElementBodyWriter::text(const std::string &content) const {
  output_ << content;
  return *this;
}

HtmlElementWriter HtmlElementBodyWriter::element(const std::string &name) const {
  return HtmlElementWriter(name, output_);
}

} // namespace common
} // namespace odr
