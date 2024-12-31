#pragma once

#include <iosfwd>
#include <string>

namespace odr::internal::text {

// TODO pass max read distance
std::string guess_charset(std::istream &in);

} // namespace odr::internal::text
