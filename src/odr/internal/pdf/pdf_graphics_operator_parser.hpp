#pragma once

#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <iostream>

namespace odr::internal::pdf {

class SimpleArray;
class SimpleArrayElement;
class GraphicsArgument;
enum class GraphicsOperatorType;
struct GraphicsOperator;

class GraphicsOperatorParser {
public:
  explicit GraphicsOperatorParser(std::istream &);

  [[nodiscard]] std::istream &in() const;
  [[nodiscard]] std::streambuf &sb() const;

  [[nodiscard]] std::string read_operator_name() const;

  [[nodiscard]] GraphicsOperator read_operator() const;

private:
  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
