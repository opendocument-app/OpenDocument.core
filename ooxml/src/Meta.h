#ifndef ODR_OOXML_META_H
#define ODR_OOXML_META_H

#include <exception>
#include <memory>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
}

namespace odr {
struct FileMeta;

namespace access {
class Path;
class ReadStorage;
} // namespace access
} // namespace odr

namespace odr::ooxml {

struct NoOfficeOpenXmlFileException final : public std::exception {
  const char *what() const noexcept final { return "not a open document file"; }
};

namespace Meta {
FileMeta parseFileMeta(access::ReadStorage &storage);

std::unordered_map<std::string, std::string>
parseRelationships(const pugi::xml_document &relations);
std::unordered_map<std::string, std::string>
parseRelationships(const access::ReadStorage &storage,
                   const access::Path &path);
} // namespace Meta

} // namespace odr::ooxml

#endif // ODR_OOXML_META_H
