#include <common/constants.h>
#include <odr/open_document_reader.h>

namespace odr {

std::string OpenDocumentReder::version() noexcept {
  return common::Constants::version();
}

std::string OpenDocumentReder::commit() noexcept {
  return common::Constants::commit();
}

} // namespace odr
