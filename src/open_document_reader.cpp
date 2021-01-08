#include <common/constants.h>
#include <odr/open_document_reader.h>

namespace odr {

std::string OpenDocumentReader::version() noexcept {
  return common::Constants::version();
}

std::string OpenDocumentReader::commit() noexcept {
  return common::Constants::commit();
}

} // namespace odr
