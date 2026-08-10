#include <odr/internal/html/xml_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/xml/xml_file.hpp>

#include <pugixml.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace odr::internal::html {
namespace {

/// Not `escape_text`, which folds a run of spaces into `&nbsp;` and a tab into
/// `&emsp;`.
std::string escape_source(std::string text) {
  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");
  return text;
}

void write_span(std::ostream &out, const std::string_view clazz,
                const std::string &content) {
  out << "<span class=\"" << clazz << "\">" << content << "</span>";
}

void write_indent(std::ostream &out, const std::uint32_t depth) {
  if (depth == 0) {
    return;
  }
  write_span(out, "odr-xml-indent", std::string(std::size_t{2} * depth, ' '));
}

/// `name="value"`, single-quoted where that is the delimiter the value does not
/// carry; a value carrying both takes the double quote as `&quot;`. The file's
/// own quoting is not in the tree.
void write_attributes(std::ostream &out, const pugi::xml_node &node) {
  for (const pugi::xml_attribute &attribute : node.attributes()) {
    out << " ";
    write_span(out, "odr-xml-attr", escape_source(attribute.name()));
    out << "=";

    const std::string value = attribute.value();
    const bool single_quoted = value.find('"') != std::string::npos &&
                               value.find('\'') == std::string::npos;
    const char quote = single_quoted ? '\'' : '"';
    std::string content = escape_source(value);
    if (!single_quoted) {
      util::string::replace_all(content, "\"", "&quot;");
    }
    write_span(out, "odr-xml-value", quote + content + quote);
  }
}

void write_start_tag(std::ostream &out, const pugi::xml_node &node,
                     const bool self_closing) {
  out << "<span class=\"odr-xml-tag\">&lt;";
  write_span(out, "odr-xml-name", escape_source(node.name()));
  write_attributes(out, node);
  out << (self_closing ? "/&gt;" : "&gt;");
  out << "</span>";
}

void write_end_tag(std::ostream &out, const pugi::xml_node &node) {
  out << "<span class=\"odr-xml-tag\">&lt;/";
  write_span(out, "odr-xml-name", escape_source(node.name()));
  out << "&gt;</span>";
}

/// One node and, for an element, everything under it, on a single line.
void write_inline(std::ostream &out, const pugi::xml_node &node) {
  switch (node.type()) {
  case pugi::node_element:
    if (!node.first_child()) {
      write_start_tag(out, node, true);
      return;
    }
    write_start_tag(out, node, false);
    for (const pugi::xml_node &child : node.children()) {
      write_inline(out, child);
    }
    write_end_tag(out, node);
    return;
  case pugi::node_pcdata:
    write_span(out, "odr-xml-text", escape_source(node.value()));
    return;
  case pugi::node_cdata:
    write_span(out, "odr-xml-cdata",
               "&lt;![CDATA[" + escape_source(node.value()) + "]]&gt;");
    return;
  case pugi::node_comment:
    write_span(out, "odr-xml-comment",
               "&lt;!--" + escape_source(node.value()) + "--&gt;");
    return;
  case pugi::node_pi: {
    std::string content = "&lt;?" + escape_source(node.name());
    if (const std::string value = node.value(); !value.empty()) {
      content += " " + escape_source(value);
    }
    content += "?&gt;";
    write_span(out, "odr-xml-pi", content);
    return;
  }
  case pugi::node_declaration:
    out << "<span class=\"odr-xml-decl\">&lt;?";
    out << escape_source(node.name());
    write_attributes(out, node);
    out << "?&gt;</span>";
    return;
  case pugi::node_doctype:
    write_span(out, "odr-xml-doctype",
               "&lt;!DOCTYPE " + escape_source(node.value()) + "&gt;");
    return;
  default:
    return;
  }
}

template <typename Content>
void write_line(HtmlWriter &out, const std::uint32_t depth,
                const Content &content) {
  out.write_element_begin(
      "div", HtmlElementOptions().set_inline(true).set_class("odr-xml-line"));
  write_indent(out.out(), depth);
  content(out.out());
  out.write_element_end("div");
}

void write_node(HtmlWriter &out, const pugi::xml_node &node,
                std::uint32_t depth);

/// Nothing short of a schema tells significant whitespace from the other kind,
/// so an element holding any text keeps the shape it came in.
bool has_text_child(const pugi::xml_node &node) {
  return std::ranges::any_of(node.children(), [](const pugi::xml_node &child) {
    const pugi::xml_node_type type = child.type();
    return type == pugi::node_pcdata || type == pugi::node_cdata;
  });
}

/// Open, always: collapsed by default hides what the file was opened to see,
/// and costs find-in-page the section it would expand into.
void write_element(HtmlWriter &out, const pugi::xml_node &node,
                   const std::uint32_t depth) {
  if (!node.first_child() || has_text_child(node)) {
    write_line(out, depth,
               [&node](std::ostream &line) { write_inline(line, node); });
    return;
  }

  out.write_element_begin(
      "details",
      HtmlElementOptions().set_class("odr-xml-node").set_extra("open"));

  out.write_element_begin("summary", HtmlElementOptions().set_inline(true));
  write_indent(out.out(), depth);
  write_start_tag(out.out(), node, false);
  out.write_element_end("summary");

  for (const pugi::xml_node &child : node.children()) {
    write_node(out, child, depth + 1);
  }
  write_line(out, depth,
             [&node](std::ostream &line) { write_end_tag(line, node); });

  out.write_element_end("details");
}

void write_node(HtmlWriter &out, const pugi::xml_node &node,
                const std::uint32_t depth) {
  if (node.type() == pugi::node_element) {
    write_element(out, node, depth);
    return;
  }
  write_line(out, depth,
             [&node](std::ostream &line) { write_inline(line, node); });
}

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(std::shared_ptr<xml::XmlFile> xml_file, HtmlConfig config,
                  const Logger &logger)
      : HtmlService(std::move(config), logger), m_xml_file{std::move(xml_file)},
        m_resources{locate_xml_resources(this->config())} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "xml", 0, "xml.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    return path == "xml.html" || resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "xml.html") {
      return "text/html";
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      return resource->mime_type();
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "xml.html") {
      HtmlWriter writer(out, config());
      write_xml(writer);
      return;
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      resource->write_resource(out);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == "xml.html") {
      return write_xml(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_xml(HtmlWriter &out) const {
    HtmlResources resources;
    const WritingState state(out, config(), resources);

    const pugi::xml_document &document = m_xml_file->document();

    out.write_begin();

    out.write_header_begin();

    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    write_viewport_meta(out, config(), false);

    write_xml_style(state);

    out.write_header_end();

    out.write_body_begin();

    out.write_element_begin("div", HtmlElementOptions().set_class("odr-xml"));
    for (const pugi::xml_node &child : document.children()) {
      write_node(out, child, 0);
    }
    out.write_element_end("div");

    out.write_body_end();

    out.write_end();

    return resources;
  }

protected:
  std::shared_ptr<xml::XmlFile> m_xml_file;
  /// The css this view links; empty of locations when the config embeds it.
  HtmlResources m_resources;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_xml_service(const TextFile &text_file,
                                     HtmlConfig config, const Logger &logger) {
  std::shared_ptr<xml::XmlFile> xml_file =
      std::dynamic_pointer_cast<xml::XmlFile>(text_file.impl());
  if (xml_file == nullptr) {
    throw NoXmlFile();
  }
  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      std::move(xml_file), std::move(config), logger));
}

} // namespace odr::internal
