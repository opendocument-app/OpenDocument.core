#pragma once

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pugi {
class xml_document;
class xml_node;
} // namespace pugi

namespace odr::internal::abstract {
class File;
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal {
class AbsPath;
}

namespace odr::internal::xml {

/// Escapes `&`, `<` and `>` for element content, and drops the control
/// characters xml 1.0 cannot carry at all.
[[nodiscard]] std::string escape_text(std::string_view text);
/// As @ref escape_text, plus the `"` that would end an attribute value.
[[nodiscard]] std::string escape_attribute(std::string_view value);

/// Parses UTF-8 text, whatever encoding its declaration still names.
pugi::xml_document parse(const std::string &);
/// Detects the encoding of the bytes. Buffers @p in twice on the way in; prefer
/// the @ref abstract::File overload, which reads once against the size the file
/// knows.
pugi::xml_document parse(std::istream &);
pugi::xml_document parse(const abstract::File &);
pugi::xml_document parse(const abstract::ReadableFilesystem &, const AbsPath &);

/// Sets the attribute @p name of @p node, appending it where it is missing.
void set_attribute(pugi::xml_node node, const char *name, const char *value);
/// Inserts a child @p name into @p parent at its place in the schema
/// sequence @p order; a child the sequence does not name ranks last.
pugi::xml_node insert_in_sequence(pugi::xml_node parent, const char *name,
                                  std::span<const std::string_view> order);

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

} // namespace odr::internal::xml
