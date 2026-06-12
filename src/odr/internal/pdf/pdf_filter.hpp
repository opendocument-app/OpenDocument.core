#pragma once

#include <odr/internal/pdf/pdf_object.hpp>

#include <optional>
#include <string>

namespace odr::internal::pdf {

/// Result of decoding a stream through its `/Filter` chain (ISO 32000-1 7.4).
/// If the chain reaches an image codec (DCTDecode, JPXDecode, CCITTFaxDecode,
/// JBIG2Decode), decoding stops there: `data` holds the still-encoded payload,
/// `stopped_at_filter` the codec's canonical name and `stopped_at_parms` its
/// decode parameters.
struct DecodeResult {
  std::string data;
  std::optional<std::string> stopped_at_filter;
  Object stopped_at_parms;
};

/// `filter` and `decode_parms` are the already reference-resolved values of
/// `/Filter` and `/DecodeParms`: null, a single name/dictionary, or parallel
/// arrays.
DecodeResult decode(const Object &filter, const Object &decode_parms,
                    std::string data);

std::string ascii_hex_decode(const std::string &input);
std::string ascii85_decode(const std::string &input);
std::string lzw_decode(const std::string &input, Integer early_change = 1);
std::string run_length_decode(const std::string &input);

/// Predictor post-pass for Flate/LZW streams (ISO 32000-1 7.4.4.4).
std::string apply_predictor(std::string data, Integer predictor, Integer colors,
                            Integer bits_per_component, Integer columns);

} // namespace odr::internal::pdf
