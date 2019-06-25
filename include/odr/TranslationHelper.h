#ifndef ODR_TRANSLATIONHELPER_H
#define ODR_TRANSLATIONHELPER_H

#include <string>
#include <memory>

namespace odr {

struct FileMeta;
struct TranslationConfig;
class TranslationHelperImpl;

class TranslationHelper {
public:
    TranslationHelper();
    ~TranslationHelper();

    bool open(const std::string &);
    bool openMicrosoft(const std::string &);
    bool decrypt(const std::string &);
    const FileMeta &getMeta() const;
    bool translate(const std::string &out, const TranslationConfig &config);

private:
    TranslationHelperImpl * const impl_;
};

}

#endif //ODR_TRANSLATIONHELPER_H
