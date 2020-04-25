#include <access/FileUtil.h>
#include <iostream>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/Meta.h>
#include <string>

int main(int, char **argv) {
  const std::string input{argv[1]};
  const std::string diff{argv[2]};
  const std::string output{argv[3]};

  odr::Config config;
  config.entryOffset = 0;
  config.entryCount = 0;
  config.editable = true;

  const odr::Document document{input};

  if (document.encrypted()) {
    std::cerr << "encrypted documents are not supported" << std::endl;
    return 1;
  }

  document.translate(output, config);

  const std::string backDiff = odr::access::FileUtil::read(diff);
  document.edit(backDiff);
  document.save(output);

  return 0;
}
