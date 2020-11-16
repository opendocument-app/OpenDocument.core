#include <odr/OpenDocument.h>
#include <common/Constants.h>

namespace odr {

std::string OpenDocument::version() noexcept {
  return common::Constants::version();
}

std::string OpenDocument::commit() noexcept {
  return common::Constants::commit();
}

}
