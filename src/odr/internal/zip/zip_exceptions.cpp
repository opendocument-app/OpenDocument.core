#include <odr/internal/zip/zip_exceptions.hpp>

namespace odr::internal::zip {

MinizSaveError::MinizSaveError(mz_zip_archive &archive) {
  error = mz_zip_get_last_error(&archive);
  error_string = mz_zip_get_error_string(error);
}

MinizSaveError::MinizSaveError(mz_zip_error _error) : error{_error} {
  error_string = mz_zip_get_error_string(_error);
}

const char *MinizSaveError::what() const noexcept { return error_string; }

} // namespace odr::internal::zip
