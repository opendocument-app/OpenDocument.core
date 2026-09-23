#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::pdf {

/// The `/DecodeParms` of `CCITTFaxDecode` (ISO 32000-1 Table 11). EOL and EOFB
/// are found wherever they occur, and a damaged row fails the image.
struct CcittParameters {
  std::int32_t k{0}; ///< < 0 Group 4, 0 Group 3 1-D, > 0 Group 3 mixed
  bool encoded_byte_align{false};
  std::int32_t columns{1728};
  std::int32_t rows{0}; ///< 0: as many as the data holds
  bool black_is_1{false};
};

/// Decode ITU-T T.4 / T.6 data into 1-bit samples (ISO 32000-1 8.9.5.2). Rows
/// past the end of the data are white. `nullopt` for an invalid code or the
/// uncompressed-mode extension.
std::optional<std::string> decode_ccitt(std::string_view data,
                                        const CcittParameters &parameters);

} // namespace odr::internal::pdf
