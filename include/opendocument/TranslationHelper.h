#ifndef OPENDOCUMENT_TRANSLATIONHELPER_H
#define OPENDOCUMENT_TRANSLATIONHELPER_H

#include <string>

namespace opendocument {

struct TranslationConfig {
};

class TranslationHelper {
public:
    static TranslationHelper &instance();

    virtual TranslationConfig &getConfig() = 0;

    virtual bool translate(const std::string &in, const std::string &out) const = 0;
};

}

#endif //OPENDOCUMENT_TRANSLATIONHELPER_H
