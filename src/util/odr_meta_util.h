#ifndef ODR_META_UTIL_H
#define ODR_META_UTIL_H

#include <nlohmann/json.hpp>

namespace odr {
struct FileMeta;
}

namespace odr::util::meta {

nlohmann::json meta_to_json(const odr::FileMeta &meta);

}

#endif // ODR_META_UTIL_H
