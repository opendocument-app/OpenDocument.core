#ifndef ODR_INTERNAL_PROJECT_INFO_HPP
#define ODR_INTERNAL_PROJECT_INFO_HPP

namespace odr::internal::project_info {

const char *version() noexcept;
bool is_debug() noexcept;

bool with_wvware() noexcept;
bool with_pdf2htmlex() noexcept;
const char *fontconfig_data_path() noexcept;
const char *poppler_data_path() noexcept;
const char *pdf2htmlex_data_path() noexcept;

} // namespace odr::internal::project_info

#endif // ODR_INTERNAL_PROJECT_INFO_HPP
