#ifndef ODR_OPENDOCUMENT_H
#define ODR_OPENDOCUMENT_H

#include <string>

namespace odr {

namespace OpenDocument {
static std::string version() noexcept;
static std::string commit() noexcept;
} // namespace OpenDocument

} // namespace odr

#endif // ODR_OPENDOCUMENT_H