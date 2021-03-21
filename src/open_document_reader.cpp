#include <internal/common/constants.h>
#include <odr/open_document_reader.h>

namespace odr {

std::string OpenDocumentReader::version() noexcept {
  return internal::common::constants::version();
}

std::string OpenDocumentReader::commit() noexcept {
  return internal::common::constants::commit();
}

} // namespace odr
