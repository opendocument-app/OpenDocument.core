#include <iostream>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/Meta.h>
#include <string>

int main(int argc, char **argv) {
  const std::string input{argv[1]};
  const std::string output{argv[2]};

  bool hasPassword = argc >= 4;
  std::string password;
  if (hasPassword)
    password = argv[3];

  odr::Config config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;
  config.paging = true;

  const odr::Document document{input};

  if (document.encrypted()) {
    if (hasPassword) {
      if (!document.decrypt(password)) {
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
