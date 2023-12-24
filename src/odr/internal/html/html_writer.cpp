#include <odr/internal/html/html_writer.hpp>

#include <odr/internal/html/common.hpp>

#include <iostream>
#include <stdexcept>

namespace odr::internal::html {

HtmlWriter::HtmlWriter(std::ostream &out, bool format, std::uint8_t indent)
    : m_out{out}, m_format{format}, m_indent(indent, ' ') {}

void HtmlWriter::write_begin() {
  m_out << "<!DOCTYPE html>\n";
  m_out << "<html>";
}

void HtmlWriter::write_end() {
  write_new_line();

  m_out << "</html>";
}

void HtmlWriter::write_header_begin() {
  write_new_line();
  ++m_current_indent;

  m_out << "<head>";
}

void HtmlWriter::write_header_end() {
  --m_current_indent;
  write_new_line();

  m_out << "</head>";
}

void HtmlWriter::write_header_title(const std::string &title) {
  write_new_line();

  m_out << "<title>" << title << "</title>";
}

void HtmlWriter::write_header_viewport(const std::string &viewport) {
  write_new_line();

  m_out << "<meta name=\"viewport\" content=\"";
  m_out << viewport;
  m_out << "\"/>";
}

void HtmlWriter::write_header_target(const std::string &target) {
  write_new_line();

  m_out << "<base target=\"";
  m_out << target;
  m_out << "\"/>";
}

void HtmlWriter::write_header_charset(const std::string &charset) {
  write_new_line();

  m_out << "<meta charset=\"";
  m_out << charset;
  m_out << "\"/>";
}

void HtmlWriter::write_header_style(const std::string &href) {
  write_new_line();

  m_out << "<link rel=\"stylesheet\" href=\"";
  m_out << href;
  m_out << "\"/>";
}

void HtmlWriter::write_header_style_begin() {
  write_new_line();
  ++m_current_indent;

  m_out << "<style>";
}

void HtmlWriter::write_header_style_end() {
  --m_current_indent;
  write_new_line();

  m_out << "</style>";
}

void HtmlWriter::write_script(const std::string &src) {
  write_new_line();

  m_out << "<script type=\"text/javascript\" src=\"" << src << "\"></script>";
}

void HtmlWriter::write_script_begin() {
  write_new_line();
  ++m_current_indent;

  m_out << "<script>";
}

void HtmlWriter::write_script_end() {
  --m_current_indent;
  write_new_line();

  m_out << "</script>";
}

void HtmlWriter::write_body_begin(HtmlElementOptions options) {
  write_new_line();
  ++m_current_indent;

  m_out << "<body";

  if (!options.clazz.empty()) {
    m_out << " class=\"" << options.clazz << "\"";
  }
  if (!options.style.empty()) {
    m_out << " style=\"" << options.style << "\"";
  }
  for (const auto &[key, value] : options.attributes) {
    m_out << " " << key << "=\"" << value << "\"";
  }
  if (!options.extra.empty()) {
    m_out << " " << options.extra;
  }

  m_out << ">";
}

void HtmlWriter::write_body_end() {
  --m_current_indent;
  write_new_line();

  m_out << "</body>";
}

void HtmlWriter::write_element_begin(const std::string &name,
                                     HtmlElementOptions options) {
  write_new_line();
  if (options.close_type == HtmlCloseType::standard) {
    ++m_current_indent;
    m_stack.push_back({name, options.inline_element});
  }

  m_out << "<" << name;

  if (!options.clazz.empty()) {
    m_out << " class=\"" << options.clazz << "\"";
  }
  if (!options.style.empty()) {
    m_out << " style=\"" << options.style << "\"";
  }
  for (const auto &[key, value] : options.attributes) {
    m_out << " " << key << "=\"" << value << "\"";
  }
  if (!options.extra.empty()) {
    m_out << " " << options.extra;
  }

  if (options.close_type == HtmlCloseType::trailing) {
    m_out << "/>";
  } else {
    m_out << ">";
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

  m_out << "</" << name << ">";
}

bool HtmlWriter::is_inline_mode() const {
  for (const auto &element : m_stack) {
    if (element.inline_element) {
      return true;
    }
  }
  return false;
}

void HtmlWriter::write_new_line() {
  if (!m_format) {
    return;
  }
  if (is_inline_mode()) {
    return;
  }

  m_out << '\n';
  for (std::uint32_t i = 0; i < m_current_indent; ++i) {
    m_out << m_indent;
  }
}

std::ostream &HtmlWriter::out() { return m_out; }

} // namespace odr::internal::html
