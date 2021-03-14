#include <iostream>
#include <odr/document.h>
#include <odr/html_config.h>
#include <optional>
#include <string>

using namespace odr;

int main(int argc, char **argv) {
  const std::string input{argv[1]};
  const std::string output{argv[2]};

  std::optional<std::string> password;
  if (argc >= 4) {
    password = argv[3];
  }

  HtmlConfig config;
  config.entry_offset = 0;
  config.entry_count = 0;
  config.editable = true;

  const odr::Document document{input};

  if (document.encrypted()) {
    if (password) {
      if (!document.decrypt(*password)) {
        std::cerr << "wrong password" << std::endl;
        return 1;
      }
    } else {
      std::cerr << "document encrypted but no password given" << std::endl;
      return 2;
    }
  }

  document.translate(output, config);

  return 0;
}
