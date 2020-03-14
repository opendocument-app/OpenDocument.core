#ifndef ODR_OFFICEOPENXMLMETA_H
#define ODR_OFFICEOPENXMLMETA_H

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
class Storage;
} // namespace access
} // namespace odr

namespace odr {
namespace ooxml {

class NoOfficeOpenXmlFileException : public std::exception {
public:
  const char *what() const noexcept override {
    return "not a open document file";
  }

private:
};

namespace OfficeOpenXmlMeta {

extern FileMeta parseFileMeta(access::Storage &storage);

extern access::Path relationsPath(const access::Path &path);
extern std::unique_ptr<tinyxml2::XMLDocument>
loadRelationships(access::Storage &storage, const access::Path &path);
extern std::unordered_map<std::string, std::string>
parseRelationships(const tinyxml2::XMLDocument &relations);
extern std::unordered_map<std::string, std::string>
parseRelationships(access::Storage &storage, const access::Path &path);

} // namespace OfficeOpenXmlMeta

} // namespace ooxml
} // namespace odr

#endif // ODR_OFFICEOPENXMLMETA_H
