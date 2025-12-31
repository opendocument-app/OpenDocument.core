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
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
