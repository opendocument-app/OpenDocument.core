#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/Document.h>
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

  const auto document = odr::Document::open(input);
  if (!document)
    return 1;

  if (document->encrypted() && hasPassword) {
    success = document->decrypt(password);
    if (!success)
      return 2;
  }

  success = document->translate(output, config);
  if (!success)
    return 3;

  return 0;
}
