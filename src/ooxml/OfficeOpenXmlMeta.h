#ifndef ODR_OFFICEOPENXMLMETA_H
#define ODR_OFFICEOPENXMLMETA_H

#include <memory>
#include <string>
#include <exception>
#include <unordered_map>
#include "../io/Path.h"

namespace tinyxml2 {
class XMLDocument;
}

namespace odr {

struct FileMeta;
class Storage;

class NoOfficeOpenXmlFileException : public std::exception {
public:
    const char *what() const noexcept override { return "not a open document file"; }
private:
};

namespace OfficeOpenXmlMeta {

extern FileMeta parseFileMeta(Storage &);

extern Path relationsPath(const Path &);
extern std::unique_ptr<tinyxml2::XMLDocument> loadRelationships(Storage &, const Path &);
extern std::unordered_map<std::string, Path> parseRelationships(const tinyxml2::XMLDocument &rels);
extern std::unordered_map<std::string, Path> parseRelationships(Storage &, const Path &);

}

}

#endif //ODR_OFFICEOPENXMLMETA_H
