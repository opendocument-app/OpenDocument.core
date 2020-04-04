#include <iostream>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/OpenDocumentReader.h>
#include <string>

int main(int argc, char **argv) {
  const std::string input(argv[1]);
  const std::string output(argv[2]);

  bool hasPassword = argc >= 4;
  std::string password;
  if (hasPassword)
    password = argv[3];

  odr::Config config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;

  bool success;

  odr::OpenDocumentReader odr;
  success = odr.open(input);
  if (!success)
    return 1;

  if (odr.getMeta().encrypted && hasPassword) {
    success = odr.decrypt(password);
    if (!success)
      return 2;
  }

  success = odr.translate(output, config);
  if (!success)
    return 3;

  odr.close();
  return 0;
}
