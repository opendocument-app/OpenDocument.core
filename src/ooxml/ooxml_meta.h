#ifndef ODR_OOXML_META_H
#define ODR_OOXML_META_H

#include <memory>
#include <stdexcept>
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

struct NoOfficeOpenXmlFileException final : public std::runtime_error {
  NoOfficeOpenXmlFileException()
      : std::runtime_error("not a open document file") {}
};

FileMeta parseFileMeta(access::ReadStorage &storage);

std::unordered_map<std::string, std::string>
parseRelationships(const pugi::xml_document &relations);
std::unordered_map<std::string, std::string>
parseRelationships(const access::ReadStorage &storage,
                   const access::Path &path);

} // namespace odr::ooxml

#endif // ODR_OOXML_META_H
