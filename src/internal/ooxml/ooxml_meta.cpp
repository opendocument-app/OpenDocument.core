#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/ooxml/ooxml_meta.h>
#include <internal/util/xml_util.h>
#include <odr/file.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml {

FileMeta parse_file_meta(abstract::ReadableFilesystem &filesystem) {
  static const std::unordered_map<common::Path, FileType> TYPES = {
      {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
      {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
      {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
  };

  FileMeta result;

  if (filesystem.is_file("/EncryptionInfo") &&
      filesystem.is_file("/EncryptedPackage")) {
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
    const std::string r_id = e.node().attribute("Id").as_string();
    const std::string p = e.node().attribute("Target").as_string();
    result.insert({r_id, p});
  }
  return result;
}

std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const common::Path &path) {
  const auto rel_path =
      path.parent().join("_rels").join(path.basename() + ".rels");
  if (!filesystem.is_file(rel_path)) {
    return {};
  }

  const auto relationships = util::xml::parse(filesystem, rel_path);
  return parse_relationships(relationships);
}

} // namespace odr::internal::ooxml
