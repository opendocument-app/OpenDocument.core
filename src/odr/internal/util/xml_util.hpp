#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace pugi {
class xml_document;
} // namespace pugi

namespace odr::internal::abstract {
class File;
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal {
class AbsPath;
}

namespace odr::internal::util::xml {

pugi::xml_document parse(const std::string &);
/// Buffers @p in twice on the way in; prefer the @ref abstract::File overload,
/// which reads once against the size the file knows.
pugi::xml_document parse(std::istream &);
pugi::xml_document parse(const abstract::File &);
pugi::xml_document parse(const abstract::ReadableFilesystem &, const AbsPath &);

/// Throws unless @p in holds a well formed xml document.
void check_xml_file(std::istream &in);

/// The `encoding` pseudo-attribute of an `<?xml …?>` declaration at the head of
/// @p in, empty if there is none. Ascii only - utf-16 and utf-32 are named by
/// their byte order mark.
[[nodiscard]] std::string read_declared_encoding(std::istream &in);

struct StringToken {
  enum class Type {
    none,
    string,
    spaces,
    tabs,
  };

  Type type;
  std::string string;

  StringToken(Type type, std::string string);
};

std::vector<StringToken> tokenize_text(const std::string &text);

} // namespace odr::internal::util::xml
