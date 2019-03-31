#ifndef ODR_CONTEXT_H
#define ODR_CONTEXT_H

#include <string>
#include <list>
#include <unordered_map>

namespace odr {

struct TranslationConfig;

struct Context {
    const TranslationConfig *config;
    std::unordered_map<std::string, std::list<std::string>> styleDependencies;
};

}

#endif //ODR_CONTEXT_H
