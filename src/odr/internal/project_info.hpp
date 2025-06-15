#pragma once

namespace odr::internal::project_info {

const char *version() noexcept;
bool is_debug() noexcept;

bool has_wvware() noexcept;
bool has_pdf2htmlex() noexcept;
const char *odr_data_path() noexcept;
const char *fontconfig_data_path() noexcept;
const char *poppler_data_path() noexcept;
const char *pdf2htmlex_data_path() noexcept;

} // namespace odr::internal::project_info
