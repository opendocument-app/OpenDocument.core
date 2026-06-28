#pragma once

#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <string>
#include <vector>

namespace odr::internal::pdf {

class Dictionary;
class Object;
struct GraphicsOperator;

class GraphicsOperatorParser {
public:
  explicit GraphicsOperatorParser(std::istream &);

  [[nodiscard]] std::istream &in();
  [[nodiscard]] std::streambuf &sb();

  [[nodiscard]] std::string read_operator_name();

  [[nodiscard]] GraphicsOperator read_operator();

private:
  // Fold an inline image's flat name/value argument run into a dictionary
  // (8.9.7); abbreviated keys are normalized to long forms downstream.
  [[nodiscard]] static Dictionary
  read_inline_image_dictionary(const std::vector<Object> &arguments);

  // Read the binary image data of an inline image, from just after the `ID`
  // keyword up to (excluding) its `EI` terminator (8.9.7), leaving the cursor
  // past `EI`. The dictionary fixes the byte length for unfiltered
  // device-colour images; otherwise the data is scanned for the terminator.
  [[nodiscard]] std::string
  read_inline_image_data(const Dictionary &dictionary);

  ObjectParser m_parser;
};

} // namespace odr::internal::pdf
