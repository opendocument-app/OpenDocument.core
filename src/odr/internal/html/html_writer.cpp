#include <odr/internal/html/html_writer.hpp>

#include <odr/internal/html/common.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace odr::internal::html {

namespace {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

bool is_empty(const HtmlWritable &writable) {
  return std::visit(
      overloaded{
          [](const char *str) { return std::strcmp(str, "") == 0; },
          [](const std::string &str) { return str.empty(); },
          [](const HtmlWriteCallback &) { return false; },
      },
      writable);
}

void write_writable(std::ostream &out, const HtmlWritable &writable) {
  std::visit(overloaded{
                 [&out](const char *str) { out << str; },
                 [&out](const std::string &str) { out << str; },
                 [&out](const HtmlWriteCallback &clb) { clb(out); },
             },
             writable);
}

void write_key_value(std::ostream &out, const HtmlWritable &key,
                     const HtmlWritable &value) {
  out << " ";
  write_writable(out, key);
  out << "=\"";
  write_writable(out, value);
  out << "\"";
}

void write_attributes(std::ostream &out, const HtmlAttributes &attributes) {
  std::visit(overloaded{
                 [&out](const HtmlAttributesVector &vector) {
                   for (const auto &[key, value] : vector) {
                     write_key_value(out, key, value);
                   }
                 },
                 [&out](const HtmlAttributeCallback &callback) {
                   callback([&out](const HtmlWritable &key,
                                   const HtmlWritable &value) {
                     write_key_value(out, key, value);
                   });
                 },
             },
             attributes);
}

void write_element_options(std::ostream &out,
                           const HtmlElementOptions &options) {
  if (options.clazz && !is_empty(*options.clazz)) {
    out << " class=\"";
    write_writable(out, *options.clazz);
    out << "\"";
  }
  if (options.style && !is_empty(*options.style)) {
    out << " style=\"";
    write_writable(out, *options.style);
    out << "\"";
  }
  if (options.attributes) {
    write_attributes(out, *options.attributes);
  }
  if (options.extra) {
    out << " ";
    write_writable(out, *options.extra);
  }
}

} // namespace

HtmlElementOptions &HtmlElementOptions::set_inline(const bool _inline_element) {
  inline_element = _inline_element;
  return *this;
}

HtmlElementOptions &
HtmlElementOptions::set_close_type(const HtmlCloseType _close_type) {
  close_type = _close_type;
  return *this;
}

HtmlElementOptions &
HtmlElementOptions::set_attributes(std::optional<HtmlAttributes> _attributes) {
  attributes = std::move(_attributes);
  return *this;
}

HtmlElementOptions &
HtmlElementOptions::set_style(std::optional<HtmlWritable> _style) {
  style = std::move(_style);
  return *this;
}

HtmlElementOptions &
HtmlElementOptions::set_class(std::optional<HtmlWritable> _class) {
  clazz = std::move(_class);
  return *this;
}

HtmlElementOptions &
HtmlElementOptions::set_extra(std::optional<HtmlWritable> _extra) {
  extra = std::move(_extra);
  return *this;
}

HtmlWriter::HtmlWriter(std::ostream &out, const bool format,
                       const std::uint8_t indent,
                       const std::uint32_t current_indent)
    : m_out{&out}, m_format{format}, m_indent(indent, ' '),
      m_current_indent{current_indent} {}

HtmlWriter::HtmlWriter(std::ostream &out, const HtmlConfig &config)
    : HtmlWriter{out, config.format_html, config.html_indent} {}

void HtmlWriter::write_begin() {
  out() << "<!DOCTYPE html>\n";
  out() << "<html>";
}

void HtmlWriter::write_end() {
  write_new_line();

  out() << "</html>";
}

void HtmlWriter::write_header_begin() {
  write_new_line();
  ++m_current_indent;

  out() << "<head>";
}

void HtmlWriter::write_header_end() {
  --m_current_indent;
  write_new_line();

  out() << "</head>";
}

void HtmlWriter::write_header_title(const std::string &title) {
  write_new_line();

  out() << "<title>" << title << "</title>";
}

void HtmlWriter::write_header_meta(const std::string &name,
                                   const std::string &content) {
  write_new_line();

  out() << R"(<meta name=")";
  out() << name;
  out() << R"(" content=")";
  out() << content;
  out() << R"("/>)";
}

void HtmlWriter::write_header_viewport(const std::string &viewport) {
  write_header_meta("viewport", viewport);
}

void HtmlWriter::write_header_target(const std::string &target) {
  write_new_line();

  out() << "<base target=\"";
  out() << target;
  out() << "\"/>";
}

void HtmlWriter::write_header_charset(const std::string &charset) {
  write_new_line();

  out() << "<meta charset=\"";
  out() << charset;
  out() << "\"/>";
}

void HtmlWriter::write_header_style(const std::string &href) {
  write_new_line();

  out() << R"(<link rel="stylesheet" href=")";
  out() << href;
  out() << "\"/>";
}

void HtmlWriter::write_header_style_begin() {
  write_new_line();
  ++m_current_indent;

  out() << "<style>";
}

void HtmlWriter::write_header_style_end() {
  --m_current_indent;
  write_new_line();

  out() << "</style>";
}

void HtmlWriter::write_script(const std::string &src) {
  write_new_line();

  out() << R"(<script type="text/javascript" src=")" << src << "\"></script>";
}

void HtmlWriter::write_script_begin() {
  write_new_line();
  ++m_current_indent;

  out() << "<script>";
}

void HtmlWriter::write_script_end() {
  --m_current_indent;
  write_new_line();

  out() << "</script>";
}

void HtmlWriter::write_body_begin(const HtmlElementOptions &options) {
  write_new_line();
  ++m_current_indent;

  out() << "<body";
  write_element_options(out(), options);
  out() << ">";
}

void HtmlWriter::write_body_end() {
  --m_current_indent;
  write_new_line();

  out() << "</body>";
}

void HtmlWriter::write_element_begin(const std::string &name,
                                     const HtmlElementOptions &options) {
  write_new_line();
  if (options.close_type == HtmlCloseType::standard) {
    ++m_current_indent;
    m_stack.push_back({name, options.inline_element});
  }

  out() << "<" << name;
  write_element_options(out(), options);
  if (options.close_type == HtmlCloseType::trailing) {
    out() << "/>";
  } else {
    out() << ">";
  }
}

void HtmlWriter::write_element_end(const std::string &name) {
  --m_current_indent;
  write_new_line();

  if (m_stack.empty()) {
    throw std::logic_error("stack is empty");
  }
  if (m_stack.back().name != name) {
    throw std::invalid_argument("names do not match");
  }
  m_stack.pop_back();

  out() << "</" << name << ">";
}

bool HtmlWriter::is_inline_mode() const {
  return std::ranges::any_of(m_stack, [](const StackElement &element) {
    return element.inline_element;
  });
}

void HtmlWriter::write_new_line() {
  if (!m_format) {
    return;
  }
  if (is_inline_mode()) {
    return;
  }

  out() << '\n';
  for (std::uint32_t i = 0; i < m_current_indent; ++i) {
    out() << m_indent;
  }
}

void HtmlWriter::write_raw(const HtmlWritable &writable, const bool new_line) {
  if (new_line) {
    write_new_line();
  }

  write_writable(out(), writable);
}

std::ostream &HtmlWriter::out() { return *m_out; }

} // namespace odr::internal::html
