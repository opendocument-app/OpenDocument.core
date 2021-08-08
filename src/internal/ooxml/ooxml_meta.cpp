#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/ooxml/ooxml_meta.h>
#include <internal/util/xml_util.h>
#include <odr/exceptions.h>
#include <odr/file_meta.h>
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
    result.encrypted = true;
    return result;
  }

  for (auto &&t : TYPES) {
    if (filesystem.is_file(t.first)) {
      result.type = t.second;
      break;
    }
  }

  // TODO dont load content twice (happens in case of translation)
  switch (result.type) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT:
    break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    const auto ppt = util::xml::parse(filesystem, "ppt/presentation.xml");
    result.entry_count = 0;
    for (auto &&_ : ppt.select_nodes("//p:sldId")) {
      ++result.entry_count;
      FileMeta::Entry entry;
      // TODO
      result.entries.emplace_back(entry);
    }
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto xls = util::xml::parse(filesystem, "xl/workbook.xml");
    result.entry_count = 0;
    for (auto &&e : xls.select_nodes("//sheet")) {
      ++result.entry_count;
      FileMeta::Entry entry;
      entry.name = e.node().attribute("name").as_string();
      // TODO dimension
      result.entries.emplace_back(entry);
    }
  } break;
  default:
    throw UnknownFileType();
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
