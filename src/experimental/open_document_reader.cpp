#include <internal/common/constants.h>
#include <odr/experimental/open_document_reader.h>

namespace odr::experimental {

std::string OpenDocumentReader::version() noexcept {
  return internal::common::constants::version();
}

std::string OpenDocumentReader::commit() noexcept {
  return internal::common::constants::commit();
}

} // namespace odr::experimental
