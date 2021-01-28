#include <abstract/filesystem.h>
#include <common/path.h>
#include <odr/file.h>
#include <ooxml/ooxml_meta.h>
#include <pugixml.hpp>
#include <unordered_map>
#include <util/xml_util.h>

namespace odr::ooxml {

FileMeta parse_file_meta(abstract::ReadableFilesystem &filesystem) {
  static const std::unordered_map<common::Path, FileType> TYPES = {
      {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
      {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
      {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
  };

  FileMeta result;

  if (filesystem.is_file("EncryptionInfo") &&
      filesystem.is_file("EncryptedPackage")) {
    result.type = FileType::OFFICE_OPEN_XML_ENCRYPTED;
    result.password_encrypted = true;
    return result;
  }

  for (auto &&t : TYPES) {
    if (filesystem.is_file(t.first)) {
      result.type = t.second;
      break;
    }
  }

  return result;
}

std::unordered_map<std::string, std::string>
parse_relationships(const pugi::xml_document &rels) {
  std::unordered_map<std::string, std::string> result;
  for (auto &&e : rels.select_nodes("//Relationship")) {
    const std::string rId = e.node().attribute("Id").as_string();
    const std::string p = e.node().attribute("Target").as_string();
    result.insert({rId, p});
  }
  return result;
}

std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const common::Path &path) {
  const auto relPath =
      path.parent().join("_rels").join(path.basename() + ".rels");
  if (!filesystem.is_file(relPath))
    return {};

  const auto relationships = util::xml::parse(filesystem, relPath);
  return parse_relationships(relationships);
}

} // namespace odr::ooxml
