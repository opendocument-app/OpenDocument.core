#include <string>
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "odr/OpenDocumentReader.h"
#include "io/FileUtil.h"

int main(int, char **argv) {
    const std::string input(argv[1]);
    const std::string diff(argv[3]);
    const std::string output(argv[2]);

    odr::TranslationConfig config = {};
    config.entryOffset = 0;
    config.entryCount = 0;
    config.editable = true;

    bool success;

    odr::OpenDocumentReader odr;
    success = odr.open(input);
    if (!success) return 1;

    success = odr.translate(output, config);
    if (!success) return 2;

    const std::string backDiff = odr::FileUtil::read(diff);
    success = odr.backTranslate(backDiff, output);
    if (!success) return 3;

    return 0;
}
