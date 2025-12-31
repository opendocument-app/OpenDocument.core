#pragma once

#include <odr/internal/pdf/pdf_object.hpp>
#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <iosfwd>
#include <variant>

namespace odr::internal::pdf {

class CMap;

class CMapParser {
public:
  using Token = std::variant<Object, std::string>;

  explicit CMapParser(std::istream &);

  [[nodiscard]] std::istream &in();
  [[nodiscard]] std::streambuf &sb();
  [[nodiscard]] const ObjectParser &parser();

  [[nodiscard]] CMap parse_cmap();

private:
  ObjectParser m_parser;

  [[nodiscard]] Token read_token();

  void read_codespacerange(std::uint32_t n, const CMap &cmap);
  void read_bfchar(std::uint32_t n, CMap &cmap);
  void read_bfrange(std::uint32_t n, const CMap &cmap);
};

} // namespace odr::internal::pdf
