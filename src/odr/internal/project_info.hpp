#pragma once

#include <string_view>

namespace odr::internal::project_info {

std::string_view version() noexcept;
bool is_debug() noexcept;

} // namespace odr::internal::project_info
