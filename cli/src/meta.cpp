#include <iostream>
#include <nlohmann/json.hpp>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/Reader.h>
#include <string>

namespace {
nlohmann::json meta_to_json(const odr::FileMeta &meta) {
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
  const std::string input(argv[1]);

  bool hasPassword = argc >= 4;
  std::string password;
  if (hasPassword)
    password = argv[2];

  odr::Reader reader;
  bool success = reader.open(input);
  if (!success)
    return 1;

  if (reader.encrypted() && hasPassword) {
    success = reader.decrypt(password);
    if (!success)
      return 2;
  }

  const auto json = meta_to_json(reader.meta());
  std::cout << json.dump(4) << std::endl;

  reader.close();
  return 0;
}
