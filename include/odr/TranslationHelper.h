#ifndef ODR_TRANSLATIONHELPER_H
#define ODR_TRANSLATIONHELPER_H

#include <string>
#include <memory>

namespace odr {

struct FileMeta;
struct TranslationConfig;

class TranslationHelper final {
public:
    static std::string getVersion();
    static std::string getCommit();

    TranslationHelper();
    ~TranslationHelper();

    bool open(const std::string &path) noexcept;
    const FileMeta *getMeta() const noexcept;
    bool decrypt(const std::string &password) noexcept;
    bool translate(const std::string &outPath, const TranslationConfig &config) noexcept;
    bool backTranslate(const std::string &diff, const std::string &outPath) noexcept;
    void close() noexcept;

private:
    class Impl;
    const std::unique_ptr<Impl> impl_;
};

}

#endif //ODR_TRANSLATIONHELPER_H
