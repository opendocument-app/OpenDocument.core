#pragma once

#include <nlohmann/json.hpp>

namespace odr {
struct FileMeta;
}

namespace odr::internal::util::meta {
nlohmann::json meta_to_json(const FileMeta &meta);
}
