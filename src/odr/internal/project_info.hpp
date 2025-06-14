#pragma once

namespace odr::internal::project_info {

const char *get_version() noexcept;
bool is_debug() noexcept;

bool has_wvware() noexcept;
bool has_pdf2htmlex() noexcept;
const char *get_odr_data_path() noexcept;
const char *get_fontconfig_data_path() noexcept;
const char *get_poppler_data_path() noexcept;
const char *get_pdf2htmlex_data_path() noexcept;

} // namespace odr::internal::project_info
