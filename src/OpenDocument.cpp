#include <common/Constants.h>
#include <odr/OpenDocument.h>

namespace odr {

std::string OpenDocument::version() noexcept {
  return common::Constants::version();
}

std::string OpenDocument::commit() noexcept {
  return common::Constants::commit();
}

} // namespace odr
