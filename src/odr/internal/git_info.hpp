#pragma once

namespace odr::internal::git_info {

const char *commit() noexcept;
bool is_dirty() noexcept;

} // namespace odr::internal::git_info
