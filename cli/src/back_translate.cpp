#include <access/FileUtil.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/Reader.h>
#include <string>

int main(int, char **argv) {
  const std::string input(argv[1]);
  const std::string diff(argv[2]);
  const std::string output(argv[3]);

  odr::Config config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;

  bool success;

  odr::Reader reader;
  success = reader.open(input);
  if (!success)
    return 1;

  success = reader.translate(output, config);
  if (!success)
    return 2;

  const std::string backDiff = odr::access::FileUtil::read(diff);
  success = reader.edit(backDiff);
  if (!success)
    return 3;
  success = reader.save(output);
  if (!success)
    return 4;

  reader.close();
  return 0;
}
