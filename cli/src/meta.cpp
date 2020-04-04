#include <iostream>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/OpenDocumentReader.h>
#include <string>
#include <nlohmann/json.hpp>

namespace {
std::string type_to_string(const odr::FileType type) {
  switch (type) {
  case odr::FileType::ZIP:
    return "zip";
  case odr::FileType::COMPOUND_FILE_BINARY_FORMAT:
    return "cfb";
  case odr::FileType::OPENDOCUMENT_TEXT:
    return "odr";
  case odr::FileType::OPENDOCUMENT_SPREADSHEET:
    return "ods";
  case odr::FileType::OPENDOCUMENT_PRESENTATION:
    return "odp";
  case odr::FileType::OPENDOCUMENT_GRAPHICS:
    return "odg";
  case odr::FileType::OFFICE_OPEN_XML_DOCUMENT:
    return "docx";
  case odr::FileType::OFFICE_OPEN_XML_WORKBOOK:
    return "xlsx";
  case odr::FileType::OFFICE_OPEN_XML_PRESENTATION:
    return "pptx";
  default:
    return "unnamed";
  }
}

nlohmann::json meta_to_json(const odr::FileMeta &meta) {
  nlohmann::json result{
      {"type", type_to_string(meta.type)},
      {"encrypted", meta.encrypted},
      {"entryCount", meta.entryCount},
      {"entries", nlohmann::json::array()},
  };

  if (!meta.entries.empty()) {
    for (auto &&e : meta.entries) {
      result["entries"].push_back({
                                         {"name", e.name},
                                         {"rowCount", e.rowCount},
                                         {"columnCount", e.columnCount},
                                         {"notes", e.notes},
      });
    }
  }

  return result;
}
} // namespace

int main(int argc, char **argv) {
  const std::string input(argv[1]);

  bool hasPassword = argc >= 4;
  std::string password;
  if (hasPassword)
    password = argv[2];

  odr::OpenDocumentReader odr;
  bool success = odr.open(input);
  if (!success)
    return 1;

  if (odr.getMeta().encrypted && hasPassword) {
    success = odr.decrypt(password);
    if (!success)
      return 2;
  }

  const auto json = meta_to_json(odr.getMeta());
  std::cout << json.dump(4) << std::endl;

  odr.close();
  return 0;
}
