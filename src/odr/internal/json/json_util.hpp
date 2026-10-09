#pragma once

#include <string_view>

namespace odr::internal::json {

/// Validates decoded UTF-8 without building a value tree.
void check_json_file(std::string_view text);

} // namespace odr::internal::json
