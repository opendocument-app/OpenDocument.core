#pragma once

#include <odr/file.hpp>

#include <string>
#include <string_view>

namespace odr::internal::encoding {

/// Decodes @p bytes to UTF-8. Malformed input becomes U+FFFD rather than an
/// error — a wrong guess is the expected failure here, and the caller
/// overrides the encoding.
///
/// @throws std::runtime_error if @p encoding is not decodable.
[[nodiscard]] std::string to_utf8(std::string_view bytes,
                                  TextEncoding encoding);

} // namespace odr::internal::encoding
