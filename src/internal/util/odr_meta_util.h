#ifndef ODR_INTERNAL_META_UTIL_H
#define ODR_INTERNAL_META_UTIL_H

#include <nlohmann/json.hpp>

namespace odr::experimental {
struct FileMeta;
}

namespace odr::internal::util::meta {
nlohmann::json meta_to_json(const experimental::FileMeta &meta);
}

#endif // ODR_INTERNAL_META_UTIL_H
