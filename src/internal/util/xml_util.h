#ifndef ODR_INTERNAL_UTIL_XML_H
#define ODR_INTERNAL_UTIL_XML_H

#include <exception>
#include <string>

namespace pugi {
class xml_document;
} // namespace pugi

namespace odr::internal::abstract {
class ReadStorage;
}

namespace odr::internal::common {
class Path;
}

namespace odr::internal::util::xml {
pugi::xml_document parse(const std::string &);
pugi::xml_document parse(std::istream &);
pugi::xml_document parse(const abstract::ReadableFilesystem &,
                         const common::Path &);
} // namespace odr::internal::util::xml

#endif // ODR_INTERNAL_XML_UTIL_H
