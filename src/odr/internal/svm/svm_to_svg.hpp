#pragma once

#include <iosfwd>

namespace odr {
class Logger;
}

namespace odr::internal::svm {
class SvmFile;

/// Translates to SVG and logs the actions it skips.
/// @throws MalformedSvmFile where a record is malformed or cut off.
void translate_to_svg(const SvmFile &file, std::ostream &out,
                      const Logger &logger);

} // namespace odr::internal::svm
