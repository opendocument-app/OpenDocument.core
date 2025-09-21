#pragma once

#include <odr/exceptions.hpp>

#include <miniz.h>

namespace odr::internal::zip {

struct MinizSaveError final : ZipSaveError {
  mz_zip_error error{MZ_ZIP_NO_ERROR};
  const char *error_string{nullptr};

  explicit MinizSaveError(mz_zip_archive &archive);
  explicit MinizSaveError(mz_zip_error error_code);

  [[nodiscard]] const char *what() const noexcept override;
};

} // namespace odr::internal::zip
