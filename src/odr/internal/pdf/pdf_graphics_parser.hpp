#ifndef ODR_INTERNAL_PDF_GRAPHICS_PARSER_HPP
#define ODR_INTERNAL_PDF_GRAPHICS_PARSER_HPP

#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <iostream>

namespace odr::internal::pdf {

class GraphicsParser {
public:
  GraphicsParser(std::istream &);

private:
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_GRAPHICS_PARSER_HPP
