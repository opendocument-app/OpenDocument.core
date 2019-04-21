#ifndef ODR_TRANSLATIONHELPER_H
#define ODR_TRANSLATIONHELPER_H

#include <string>
#include <memory>

namespace odr {

struct FileMeta;
struct TranslationConfig;

class TranslationHelper {
public:
    static std::unique_ptr<TranslationHelper> create();

    virtual ~TranslationHelper() = default;
    virtual bool open(const std::string &in) = 0;
    virtual bool decrypt(const std::string &password) = 0;
    virtual const FileMeta &getMeta() const = 0;
    // TODO: get progress
    virtual bool translate(const std::string &out, const TranslationConfig &config) const = 0;
};

}

#endif //ODR_TRANSLATIONHELPER_H
