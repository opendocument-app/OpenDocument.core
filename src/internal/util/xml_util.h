#ifndef ODR_INTERNAL_UTIL_XML_H
#define ODR_INTERNAL_UTIL_XML_H

#include <iosfwd>
#include <string>
#include <vector>

namespace pugi {
class xml_document;
} // namespace pugi

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::common {
class Path;
}

namespace odr::internal::util::xml {
pugi::xml_document parse(const std::string &);
pugi::xml_document parse(std::istream &);
pugi::xml_document parse(const abstract::ReadableFilesystem &,
                         const common::Path &);

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

#endif // ODR_INTERNAL_XML_UTIL_H
