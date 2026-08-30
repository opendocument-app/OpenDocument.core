#include <odr/internal/svg/svg_writer.hpp>

#include <cmath>
#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace odr::internal {

namespace {

/// Tab, line feed and carriage return are the only characters below `0x20` an
/// xml 1.0 document may contain.
bool is_xml_control(const char c) {
  const auto value = static_cast<unsigned char>(c);
  return value < 0x20 && c != '\t' && c != '\n' && c != '\r';
}

std::string escape(const std::string_view text, const bool attribute) {
  std::string result;
  result.reserve(text.size());

  for (const char c : text) {
    switch (c) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += attribute ? "&quot;" : "\"";
      break;
    default:
      if (!is_xml_control(c)) {
        result += c;
      }
      break;
    }
  }

  return result;
}

} // namespace

std::string svg::format_number(const double value) {
  if (!std::isfinite(value)) {
    return "0";
  }

  // the classic locale, because a stream imbued with a german one writes
  // `1,5`, and that is two coordinates rather than one
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << value;
  std::string result = stream.str();

  if (result.find('e') != std::string::npos) {
    // a `style` declaration is css, and css reads no exponent, so a number
    // that big is written out in full - at which point its fraction is noise
    stream.str({});
    stream << std::fixed << std::setprecision(0) << value;
    result = stream.str();
  }

  return result;
}

std::string svg::escape_text(const std::string_view text) {
  return escape(text, false);
}

std::string svg::escape_attribute(const std::string_view value) {
  return escape(value, true);
}

svg::SvgWriter::SvgWriter(std::ostream &out) : m_out{&out} {}

void svg::SvgWriter::close_tag(const bool with_content) {
  if (!m_tag_open) {
    return;
  }

  if (!m_style.empty()) {
    *m_out << " style=\"" << m_style << "\"";
    m_style.clear();
  }

  *m_out << (with_content ? ">" : " />");
  m_tag_open = false;
}

void svg::SvgWriter::write_element_begin(const std::string_view name) {
  close_tag(true);

  *m_out << "<" << name;
  m_stack.emplace_back(name);
  m_tag_open = true;
}

void svg::SvgWriter::write_element_end() {
  if (m_stack.empty()) {
    throw std::runtime_error("no element to end");
  }

  if (m_tag_open) {
    close_tag(false);
  } else {
    *m_out << "</" << m_stack.back() << ">";
  }
  m_stack.pop_back();
}

void svg::SvgWriter::write_attribute(const std::string_view name,
                                     const std::string_view value) {
  if (!m_tag_open) {
    throw std::runtime_error("no open tag to write an attribute to");
  }
  *m_out << " " << name << "=\"" << escape_attribute(value) << "\"";
}

void svg::SvgWriter::write_attribute(const std::string_view name,
                                     const double value) {
  write_attribute(name, format_number(value));
}

void svg::SvgWriter::write_style(const std::string_view property,
                                 const std::string_view value) {
  if (!m_tag_open) {
    throw std::runtime_error("no open tag to write a style to");
  }
  m_style += escape_attribute(property);
  m_style += ":";
  m_style += escape_attribute(value);
  m_style += ";";
}

void svg::SvgWriter::write_style(const std::string_view property,
                                 const double value) {
  write_style(property, format_number(value));
}

void svg::SvgWriter::write_text(const std::string_view text) {
  close_tag(true);
  *m_out << escape_text(text);
}

} // namespace odr::internal
