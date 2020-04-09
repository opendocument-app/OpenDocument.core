#include <access/FileUtil.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/OpenDocumentReader.h>
#include <string>

int main(int, char **argv) {
  const std::string input(argv[1]);
  const std::string diff(argv[3]);
  const std::string output(argv[2]);

  odr::Config config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;

  bool success;

  odr::OpenDocumentReader odr;
  success = odr.open(input);
  if (!success)
    return 1;

  success = odr.translate(output, config);
  if (!success)
    return 2;

  const std::string backDiff = odr::access::FileUtil::read(diff);
  success = odr.edit(backDiff);
  if (!success)
    return 3;
  success = odr.save(output);
  if (!success)
    return 4;

  odr.close();
  return 0;
}
