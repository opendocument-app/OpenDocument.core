#pragma once

namespace odr::internal::project_info {

const char *version() noexcept;
bool is_debug() noexcept;

bool has_libmagic() noexcept;

const char *odr_data_path() noexcept;
const char *libmagic_database_path() noexcept;

} // namespace odr::internal::project_info
