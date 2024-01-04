#ifndef ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_PARSER_HPP
#define ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_PARSER_HPP

#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <iostream>

namespace odr::internal::pdf {

class SimpleArray;
class SimpleArrayElement;
class GraphicsArgument;
struct GraphicsOperator;

class GraphicsOperatorParser {
public:
  GraphicsOperatorParser(std::istream &);

  std::istream &in() const;
  std::streambuf &sb() const;

  std::string read_operator_name() const;
  SimpleArrayElement read_array_element() const;
  SimpleArray read_array() const;

  GraphicsOperator read_operator() const;

private:
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_PARSER_HPP
