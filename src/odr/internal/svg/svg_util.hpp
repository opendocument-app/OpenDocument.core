#pragma once

#include <iosfwd>

namespace odr::internal::svg {

/// Throws unless @p in holds an xml document whose root element is `svg`.
void check_svg_file(std::istream &in);

} // namespace odr::internal::svg
