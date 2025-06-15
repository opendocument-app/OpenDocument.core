#pragma once

namespace odr::internal::git_info {

const char *commit_hash() noexcept;
bool is_dirty() noexcept;

} // namespace odr::internal::git_info
