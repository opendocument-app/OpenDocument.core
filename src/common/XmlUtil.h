#ifndef ODR_COMMON_XML_UTIL_H
#define ODR_COMMON_XML_UTIL_H

#include <exception>
#include <string>

namespace pugi {
class xml_document;
} // namespace pugi

namespace odr::access {
class Path;
class ReadStorage;
} // namespace odr::access

namespace odr::common {

struct NotXmlException final : public std::exception {
  const char *what() const noexcept override { return "not xml"; }
};

namespace XmlUtil {
pugi::xml_document parse(const std::string &);
pugi::xml_document parse(std::istream &);
pugi::xml_document parse(const access::ReadStorage &, const access::Path &);
} // namespace XmlUtil

} // namespace odr::common

#endif // ODR_COMMON_XML_UTIL_H
