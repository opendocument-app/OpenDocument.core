#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace odr::internal::pdf {

/// A JBIG2 page in the pipeline's sample layout (ISO 32000-1 8.9.5.2): 1 bit
/// per pixel, MSB first, rows padded to a byte. Already inverted from JBIG2's
/// 1-is-black to `/DeviceGray`, so the default `/Decode` applies.
struct Jbig2Image {
  std::int32_t width{0};
  std::int32_t height{0};
  std::string samples;
};

/// Decode a `JBIG2Decode` payload in the PDF embedded-stream organization
/// (ISO 32000-1 7.4.7), `globals` holding the `/JBIG2Globals` stream (empty
/// when there is none).
///
/// Covers arithmetic generic regions, symbol dictionaries and text regions.
/// `nullopt` for a malformed stream or anything else — MMR/Huffman, refinement,
/// halftone — so the caller skips the image rather than painting a wrong one.
std::optional<Jbig2Image> decode_jbig2(const std::string &data,
                                       const std::string &globals);

} // namespace odr::internal::pdf
