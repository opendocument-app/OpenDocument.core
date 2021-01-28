#ifndef ODR_UTIL_XML_H
#define ODR_UTIL_XML_H

#include <exception>
#include <string>

namespace pugi {
class xml_document;
} // namespace pugi

namespace odr::abstract {
class ReadStorage;
}

namespace odr::common {
class Path;
}

namespace odr::util::xml {
pugi::xml_document parse(const std::string &);
pugi::xml_document parse(std::istream &);
pugi::xml_document parse(const abstract::ReadableFilesystem &,
                         const common::Path &);
} // namespace odr::util::xml

#endif // ODR_COMMON_XML_UTIL_H
