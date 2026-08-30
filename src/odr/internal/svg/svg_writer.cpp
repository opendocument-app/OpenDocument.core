#include <odr/internal/svg/svg_writer.hpp>

#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <cmath>
#include <ostream>
#include <stdexcept>
#include <string>

namespace odr::internal {

std::string svg::format_number(const double value) {
  if (!std::isfinite(value)) {
    return "0";
  }
  return util::number::to_string_significant(value, 6);
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
  *m_out << " " << name << "=\"" << util::xml::escape_attribute(value) << "\"";
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
  m_style += util::xml::escape_attribute(property);
  m_style += ":";
  // a `;` the file wrote would open a declaration of its own; dropped before
  // escaping, which writes `;` of its own
  std::string sanitized(value);
  std::erase(sanitized, ';');
  m_style += util::xml::escape_attribute(sanitized);
  m_style += ";";
}

void svg::SvgWriter::write_style(const std::string_view property,
                                 const double value) {
  write_style(property, format_number(value));
}

void svg::SvgWriter::write_text(const std::string_view text) {
  close_tag(true);
  *m_out << util::xml::escape_text(text);
}

} // namespace odr::internal
