#include <ooxml/Meta.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <common/XmlUtil.h>
#include <odr/Exception.h>
#include <odr/File.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::ooxml {

FileMeta Meta::parseFileMeta(access::ReadStorage &storage) {
  static const std::unordered_map<access::Path, FileType> TYPES = {
      {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
      {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
      {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
  };

  FileMeta result;
  result.documentMeta = DocumentMeta();

  if (storage.isFile("EncryptionInfo") && storage.isFile("EncryptedPackage")) {
    result.type = FileType::OFFICE_OPEN_XML_ENCRYPTED;
    result.encryptionState = EncryptionState::ENCRYPTED;
    return result;
  }

  for (auto &&t : TYPES) {
    if (storage.isFile(t.first)) {
      result.type = t.second;
      break;
    }
  }

  // TODO dont load content twice (happens in case of translation)
  switch (result.type) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT:
    break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    const auto ppt = common::XmlUtil::parse(storage, "ppt/presentation.xml");
    result.documentMeta->entryCount = 0;
    for (auto &&e : ppt.select_nodes("//p:sldId")) {
      ++result.documentMeta->entryCount;
      DocumentMeta::Entry entry;
      // TODO
      result.documentMeta->entries.emplace_back(entry);
    }
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto xls = common::XmlUtil::parse(storage, "xl/workbook.xml");
    result.documentMeta->entryCount = 0;
    for (auto &&e : xls.select_nodes("//sheet")) {
      ++result.documentMeta->entryCount;
      DocumentMeta::Entry entry;
      entry.name = e.node().attribute("name").as_string();
      // TODO dimension
      result.documentMeta->entries.emplace_back(entry);
    }
  } break;
  default:
    throw UnknownFileType();
  }

  return result;
}

std::unordered_map<std::string, std::string>
Meta::parseRelationships(const pugi::xml_document &rels) {
  std::unordered_map<std::string, std::string> result;
  for (auto &&e : rels.select_nodes("//Relationship")) {
    const std::string rId = e.node().attribute("Id").as_string();
    const std::string p = e.node().attribute("Target").as_string();
    result.insert({rId, p});
  }
  return result;
}

std::unordered_map<std::string, std::string>
Meta::parseRelationships(const access::ReadStorage &storage,
                         const access::Path &path) {
  const auto relPath =
      path.parent().join("_rels").join(path.basename() + ".rels");
  if (!storage.isFile(relPath))
    return {};

  const auto relationships = common::XmlUtil::parse(storage, relPath);
  return parseRelationships(relationships);
}

} // namespace odr::ooxml
