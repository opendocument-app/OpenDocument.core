#ifndef ODR_OOXML_META_H
#define ODR_OOXML_META_H

#include <exception>
#include <memory>
#include <string>
#include <unordered_map>

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {
struct FileMeta;

namespace access {
class Path;
class ReadStorage;
} // namespace access
} // namespace odr

namespace odr {
namespace ooxml {

struct NoOfficeOpenXmlFileException final : public std::exception {
  const char *what() const noexcept final { return "not a open document file"; }
};

namespace Meta {
FileMeta parseFileMeta(access::ReadStorage &storage);

access::Path relationsPath(const access::Path &path);
std::unique_ptr<tinyxml2::XMLDocument>
loadRelationships(const access::ReadStorage &storage, const access::Path &path);
std::unordered_map<std::string, std::string>
parseRelationships(const tinyxml2::XMLDocument &relations);
std::unordered_map<std::string, std::string>
parseRelationships(const access::ReadStorage &storage,
                   const access::Path &path);
} // namespace Meta

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_META_H
