#include "OfficeOpenXmlMeta.h"
#include <unordered_map>
#include "tinyxml2.h"
#include "odr/FileMeta.h"
#include "../XmlUtil.h"
#include "../io/Storage.h"

namespace odr {

FileMeta OfficeOpenXmlMeta::parseFileMeta(Storage &storage) {
    static const std::unordered_map<Path, FileType> TYPES = {
            {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
            {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
            {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
    };

    FileMeta result{};

    for (auto &&t : TYPES) {
        if (storage.isFile(t.first)) {
            result.type = t.second;
            break;
        }
    }

    return result;
}

Path OfficeOpenXmlMeta::relationsPath(const Path &path) {
    return path.parent().join("_rels").join(path.basename() + ".rels");
}

std::unique_ptr<tinyxml2::XMLDocument> OfficeOpenXmlMeta::loadRelationships(Storage &storage, const Path &path) {
    return XmlUtil::parse(storage, relationsPath(path));
}

std::unordered_map<std::string, std::string> OfficeOpenXmlMeta::parseRelationships(const tinyxml2::XMLDocument &rels) {
    std::unordered_map<std::string, std::string> result;
    XmlUtil::recursiveVisitElementsWithName(rels.RootElement(), "Relationship", [&](const auto &rel) {
        const std::string rId = rel.FindAttribute("Id")->Value();
        const std::string p = rel.FindAttribute("Target")->Value();
        result.insert({rId, p});
    });
    return result;
}

std::unordered_map<std::string, std::string> OfficeOpenXmlMeta::parseRelationships(Storage &storage, const Path &path) {
    const auto relationships = loadRelationships(storage, path);
    if (!relationships) return {};
    return parseRelationships(*relationships);
}

}
