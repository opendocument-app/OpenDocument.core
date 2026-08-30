#pragma once

#include <iosfwd>

namespace odr {
class Logger;
}

namespace odr::internal::svm {
class SvmFile;

/// Translates @p file's actions into an svg document. Actions we do not
/// implement are logged and dropped.
/// @throws MalformedSvmFile where an action reads past its own length.
void translate_to_svg(const SvmFile &file, std::ostream &out,
                      const Logger &logger);

} // namespace odr::internal::svm
