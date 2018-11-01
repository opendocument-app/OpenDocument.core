#include "TextTranslator.h"

namespace opendocument {

TextTranslator::TextTranslator(const Config &config) :
    Translator(config),
    _config(config) {
}

TextTranslator::~TextTranslator() {}

}
