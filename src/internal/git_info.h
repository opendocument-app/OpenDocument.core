#ifndef ODR_INTERNAL_GIT_INFO_H
#define ODR_INTERNAL_GIT_INFO_H

namespace odr::internal::git_info {
const char *commit() noexcept;
bool is_dirty() noexcept;
} // namespace odr::internal::git_info

#endif // ODR_INTERNAL_GIT_INFO_H
