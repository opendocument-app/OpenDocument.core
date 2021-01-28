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
}

namespace odr::abstract {
class ReadableFilesystem;
} // namespace odr::abstract

namespace odr::common {
class Path;
} // namespace odr::common

namespace odr::ooxml {

struct NoOfficeOpenXmlFileException final : public std::runtime_error {
  NoOfficeOpenXmlFileException()
      : std::runtime_error("not a open document file") {}
};

FileMeta parse_file_meta(abstract::ReadableFilesystem &filesystem);

std::unordered_map<std::string, std::string>
parse_relationships(const pugi::xml_document &relations);

std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const common::Path &path);

} // namespace odr::ooxml

#endif // ODR_OOXML_META_H
