#include <string>
#include "odr/TranslationConfig.h"
#include "odr/TranslationHelper.h"

int main(int argc, char **argv) {
    const std::string input(argv[1]);
    const std::string output(argv[2]);
    const std::string password("password");

    odr::TranslationConfig config = {};
    config.editable = true;
    config.entryOffset = 0;
    config.entryCount = 0;

    odr::TranslationHelper translator;
    translator.open(input);
    translator.decrypt(password);
    translator.translate(output, config);

    return 0;
}
