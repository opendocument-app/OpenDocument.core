#ifndef ODR_INTERNAL_ZIP_ZIP_EXCEPTIONS_HPP
#define ODR_INTERNAL_ZIP_ZIP_EXCEPTIONS_HPP

#include <odr/exceptions.hpp>

#include <miniz.h>

namespace odr::internal::zip {

struct MinizSaveError : public ZipSaveError {
  mz_zip_error error{mz_zip_error::MZ_ZIP_NO_ERROR};
  const char *error_string{nullptr};

  MinizSaveError(mz_zip_archive &archive);
  MinizSaveError(mz_zip_error error_code);

  const char *what() const noexcept override;
};

} // namespace odr::internal::zip

#endif // ODR_INTERNAL_ZIP_ZIP_EXCEPTIONS_HPP
