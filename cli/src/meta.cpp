#include <iostream>
#include <nlohmann/json.hpp>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/Meta.h>
#include <string>

namespace {
nlohmann::json metaToJson(const odr::FileMeta &meta) {
  nlohmann::json result{
      {"type", meta.typeAsString()},
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
  const std::string input{argv[1]};

  bool hasPassword = argc >= 4;
  std::string password;
  if (hasPassword)
    password = argv[2];

  const odr::Document document{input};

  if (document.encrypted() && hasPassword) {
    if (!document.decrypt(password)) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  const auto json = metaToJson(document.meta());
  std::cout << json.dump(4) << std::endl;

  return 0;
}
