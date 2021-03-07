#include <internal/common/constants.h>
#include <odr/experimental/open_document_reader.h>

namespace odr::experimental {

std::string OpenDocumentReader::version() noexcept {
  return internal::common::Constants::version();
}

std::string OpenDocumentReader::commit() noexcept {
  return internal::common::Constants::commit();
}

} // namespace odr::experimental
