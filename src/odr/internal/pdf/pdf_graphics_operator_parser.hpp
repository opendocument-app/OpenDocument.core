#pragma once

#include <odr/internal/pdf/pdf_object_parser.hpp>

namespace odr::internal::pdf {

struct GraphicsOperator;

class GraphicsOperatorParser {
public:
  explicit GraphicsOperatorParser(std::istream &);

  [[nodiscard]] std::istream &in();
  [[nodiscard]] std::streambuf &sb();

  [[nodiscard]] std::string read_operator_name();

  [[nodiscard]] GraphicsOperator read_operator();

private:
  // Consume the binary image data of an inline image, from just after the `ID`
  // keyword up to and including its `EI` terminator (8.9.7).
  void skip_inline_image_data();

  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
