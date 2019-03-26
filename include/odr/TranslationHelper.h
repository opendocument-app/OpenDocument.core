#ifndef ODR_TRANSLATIONHELPER_H
#define ODR_TRANSLATIONHELPER_H

#include <string>
#include <memory>

namespace odr {

struct TranslationConfig;

class TranslationHelper {
public:
    static std::unique_ptr<TranslationHelper> create(const TranslationConfig &);

    virtual ~TranslationHelper() = default;
    virtual bool translate(const std::string &in, const std::string &out) const = 0;
};

}

#endif //ODR_TRANSLATIONHELPER_H
