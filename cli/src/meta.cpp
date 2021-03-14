#include <internal/util/odr_meta_util.h>
#include <iostream>
#include <odr/document.h>
#include <optional>
#include <string>

using namespace odr;

int main(int argc, char **argv) {
  const std::string input{argv[1]};

  std::optional<std::string> password;
  if (argc >= 3) {
    password = argv[2];
  }

  const odr::Document document{input};

  if (document.encrypted() && password) {
    if (!document.decrypt(*password)) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  const auto json =
      odr::internal::util::meta::meta_to_json(document.file_meta());
  std::cout << json.dump(4) << std::endl;

  return 0;
}
