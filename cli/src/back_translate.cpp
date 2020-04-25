#include <access/FileUtil.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/Document.h>
#include <string>

int main(int, char **argv) {
  const std::string input(argv[1]);
  const std::string diff(argv[2]);
  const std::string output(argv[3]);

  odr::Config config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;

  const auto document = odr::Document::open(input);
  if (!document)
    return 1;

  bool success = document->translate(output, config);
  if (!success)
    return 2;

  const std::string backDiff = odr::access::FileUtil::read(diff);
  success = document->edit(backDiff);
  if (!success)
    return 3;
  success = document->save(output);
  if (!success)
    return 4;

  return 0;
}
