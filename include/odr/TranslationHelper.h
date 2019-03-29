#ifndef ODR_TRANSLATIONHELPER_H
#define ODR_TRANSLATIONHELPER_H

#include <string>
#include <memory>

namespace odr {

struct TranslationConfig;

class TranslationHelper {
public:
    static std::unique_ptr<TranslationHelper> create();

    virtual ~TranslationHelper() = default;
    // TODO: open (with path, password, ...) return error (no odx, wrong passwort, ...)
    virtual bool open(const std::string &in) = 0;
    // TODO: read meta (page count, sheet names, ...)
    // TODO: get progress
    virtual bool translate(const std::string &out, const TranslationConfig &config) const = 0;
};

}

#endif //ODR_TRANSLATIONHELPER_H
