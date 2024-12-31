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

  std::istream &in() const;
  std::streambuf &sb() const;
  const ObjectParser &parser() const;

  CMap parse_cmap() const;

private:
  ObjectParser m_parser;

  Token read_token() const;

  void read_codespacerange(std::uint32_t n, CMap &) const;
  void read_bfchar(std::uint32_t n, CMap &) const;
  void read_bfrange(std::uint32_t n, CMap &) const;
};

} // namespace odr::internal::pdf
