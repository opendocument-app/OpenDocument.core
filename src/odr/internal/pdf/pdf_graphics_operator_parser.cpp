#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>

#include <odr/internal/pdf/pdf_graphics_operator.hpp>

namespace odr::internal::pdf {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

GraphicsOperatorParser::GraphicsOperatorParser(std::istream &in)
    : m_parser(in) {}

std::istream &GraphicsOperatorParser::in() const { return m_parser.in(); }

std::streambuf &GraphicsOperatorParser::sb() const { return m_parser.sb(); }

std::string GraphicsOperatorParser::read_operator_name() const {
  std::string result;

  while (true) {
    int_type c = sb().sgetc();

    if (c == eof) {
      in().setstate(std::ios::eofbit);
      return result;
    }
    if (c == ' ' || c == '\n' || c == '/' || c == '<' || c == '[') {
      return result;
    }

    sb().sbumpc();
    result += (char_type)c;
  }
}

SimpleArrayElement GraphicsOperatorParser::read_array_element() const {
  if (m_parser.peek_number()) {
    return std::visit([&](auto v) { return SimpleArrayElement(v); },
                      m_parser.read_integer_or_real());
  }
  if (m_parser.peek_name()) {
    return m_parser.read_name();
  }
  if (m_parser.peek_string()) {
    return m_parser.read_string();
  }

  throw std::runtime_error("unknown element");
}

SimpleArray GraphicsOperatorParser::read_array() const {
  SimpleArray::Holder result;

  if (sb().sbumpc() != '[') {
    throw std::runtime_error("unexpected character");
  }
  m_parser.skip_whitespace();

  while (true) {
    if (int_type c = sb().sgetc(); c == ']') {
      sb().sbumpc();
      return SimpleArray(std::move(result));
    }
    SimpleArrayElement value = read_array_element();
    m_parser.skip_whitespace();

    result.emplace_back(std::move(value));
  }
}

GraphicsOperator GraphicsOperatorParser::read_operator() const {
  GraphicsOperator result;

  while (true) {
    if (m_parser.peek_number()) {
      std::visit([&](auto v) { result.arguments.push_back(v); },
                 m_parser.read_integer_or_real());
    } else if (m_parser.peek_name()) {
      result.arguments.push_back(m_parser.read_name());
    } else if (m_parser.peek_string()) {
      result.arguments.push_back(m_parser.read_string());
    } else if (m_parser.peek_array()) {
      result.arguments.push_back(read_array());
    } else {
      m_parser.skip_whitespace();
      break;
    }
    m_parser.skip_whitespace();
  }

  result.name = read_operator_name();
  m_parser.skip_whitespace();

  return result;
}

} // namespace odr::internal::pdf
