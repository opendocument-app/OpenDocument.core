#pragma once

#include <iosfwd>

namespace odr {
class Logger;
}

namespace odr::internal::svm {
class SvmFile;

/// Translates to SVG, logging skipped actions and rejecting malformed records.
void translate_to_svg(const SvmFile &file, std::ostream &out,
                      const Logger &logger);

} // namespace odr::internal::svm
