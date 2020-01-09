#include <string>
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

int main(int argc, char **argv) {
    const std::string input(argv[1]);
    const std::string output(argv[2]);

    bool hasPassword = argc >= 4;
    std::string password;
    if (hasPassword) password = argv[3];

    odr::TranslationConfig config = {};
    config.entryOffset = 0;
    config.entryCount = 0;
    config.editable = true;

    bool success = true;

    odr::TranslationHelper translator;
    success &= translator.openOpenDocument(input);
    if (hasPassword) {
        success &= translator.decrypt(password);
    }
    success &= translator.translate(output, config);

    return !success;
}
