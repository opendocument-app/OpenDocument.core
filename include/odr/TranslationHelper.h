#ifndef ODR_TRANSLATIONHELPER_H
#define ODR_TRANSLATIONHELPER_H

#include <string>
#include <memory>

namespace odr {

struct FileMeta;
struct TranslationConfig;

class TranslationHelper final {
public:
    TranslationHelper();
    ~TranslationHelper();

    bool openOpenDocument(const std::string &) noexcept;
    bool openMicrosoft(const std::string &) noexcept;
    bool decrypt(const std::string &) noexcept;
    const FileMeta *getMeta() const noexcept;
    bool translate(const std::string &out, const TranslationConfig &config) noexcept;
    void close() noexcept;

private:
    class Impl;
    const std::unique_ptr<Impl> impl_;
};

}

#endif //ODR_TRANSLATIONHELPER_H
