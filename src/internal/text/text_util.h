#ifndef ODR_INTERNAL_TEXT_UTIL_H
#define ODR_INTERNAL_TEXT_UTIL_H

#include <iosfwd>
#include <string>

namespace odr::internal::text {

// TODO pass max read distance
std::string guess_charset(std::istream &in);

} // namespace odr::internal::text

#endif // ODR_INTERNAL_TEXT_UTIL_H
