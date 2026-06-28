#include <odr/internal/pdf/pdf_image.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_color.hpp>
#include <odr/internal/pdf/pdf_filter.hpp>
#include <odr/internal/pdf/pdf_object.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>

namespace odr::internal::pdf {

namespace {

/// Append a PNG chunk: length, four-byte type, data, CRC over type+data.
void write_chunk(std::string &out, const char (&type)[5],
                 const std::string &data) {
  util::byte_string::put_u32_be(out, static_cast<std::uint32_t>(data.size()));
  const std::size_t crc_start = out.size();
  out.append(type, 4);
  out.append(data);
  util::byte_string::put_u32_be(
      out, crypto::util::crc32(std::string_view(out).substr(crc_start)));
}

/// Reads fixed-width big-endian sample values out of a byte buffer, MSB first.
/// Constructed at a row offset; rows are byte-aligned (8.9.5.2). Reads past the
/// end yield zero (lenient for a truncated stream).
class BitReader {
public:
  BitReader(const std::string_view data, const std::size_t byte_offset)
      : m_data{data}, m_byte{byte_offset} {}

  std::uint32_t read(const std::int32_t bits) {
    std::uint32_t value = 0;
    for (std::int32_t i = 0; i < bits; ++i) {
      value = (value << 1) | next_bit();
    }
    return value;
  }

private:
  std::uint32_t next_bit() {
    if (m_byte >= m_data.size()) {
      return 0;
    }
    const std::uint32_t bit =
        (static_cast<std::uint8_t>(m_data[m_byte]) >> (7 - m_bit)) & 1u;
    if (++m_bit == 8) {
      m_bit = 0;
      ++m_byte;
    }
    return bit;
  }

  std::string_view m_data;
  std::size_t m_byte;
  std::int32_t m_bit{0};
};

std::uint8_t to_byte(const double v) {
  const double scaled = std::lround(std::clamp(v, 0.0, 1.0) * 255.0);
  return static_cast<std::uint8_t>(scaled);
}

} // namespace

} // namespace odr::internal::pdf

namespace odr::internal {

std::string pdf::write_png(const std::string &pixels, const std::int32_t width,
                           const std::int32_t height,
                           const std::int32_t channels) {
  if (width <= 0 || height <= 0 || (channels != 3 && channels != 4)) {
    return {};
  }
  const auto stride =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
  if (pixels.size() < stride * static_cast<std::size_t>(height)) {
    return {};
  }

  // Filter type 0 (None) prefixes each scanline (PNG 9.2); the rows are then
  // deflated as one zlib stream into the single IDAT.
  std::string raw;
  raw.reserve((stride + 1) * static_cast<std::size_t>(height));
  for (std::int32_t y = 0; y < height; ++y) {
    raw.push_back(0);
    raw.append(pixels, static_cast<std::size_t>(y) * stride, stride);
  }

  std::string out;
  static const char signature[] = {
      static_cast<char>(0x89), 'P', 'N', 'G', '\r', '\n',
      static_cast<char>(0x1A), '\n'};
  out.append(signature, sizeof(signature));

  std::string ihdr;
  util::byte_string::put_u32_be(ihdr, static_cast<std::uint32_t>(width));
  util::byte_string::put_u32_be(ihdr, static_cast<std::uint32_t>(height));
  ihdr.push_back(8); // bit depth
  // Colour type: 2 = truecolour (RGB), 6 = truecolour with alpha (RGBA).
  ihdr.push_back(channels == 4 ? 6 : 2);
  ihdr.push_back(0); // compression: deflate
  ihdr.push_back(0); // filter method: adaptive
  ihdr.push_back(0); // interlace: none
  write_chunk(out, "IHDR", ihdr);
  write_chunk(out, "IDAT", crypto::util::zlib_deflate(raw));
  write_chunk(out, "IEND", "");
  return out;
}

std::string pdf::encode_image_png(const std::string &samples,
                                  const std::int32_t width,
                                  const std::int32_t height,
                                  const std::int32_t bits_per_component,
                                  const ColorSpaceDef &color_space,
                                  const std::vector<double> &decode,
                                  const std::vector<std::uint8_t> &alpha,
                                  const std::vector<double> &color_key) {
  const std::int32_t components = color_space.components;
  if (width <= 0 || height <= 0 || components <= 0 || bits_per_component <= 0 ||
      bits_per_component > 16) {
    return {};
  }

  const std::uint32_t max_sample =
      (bits_per_component >= 32)
          ? 0xFFFFFFFFu
          : ((1u << static_cast<std::uint32_t>(bits_per_component)) - 1u);
  const bool indexed = color_space.kind == ColorSpaceKind::indexed;

  const auto row_bits = static_cast<std::size_t>(width) *
                        static_cast<std::size_t>(components) *
                        static_cast<std::size_t>(bits_per_component);
  const std::size_t row_bytes = (row_bits + 7) / 8;

  const auto pixel_count =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  // A colour-key array (8.9.6.4) or an alpha plane (an SMask / stencil /Mask)
  // makes the output RGBA; otherwise it stays the compact 3-byte RGB.
  const bool has_color_key =
      color_key.size() >= 2 * static_cast<std::size_t>(components);
  const bool has_alpha = alpha.size() == pixel_count || has_color_key;
  const std::size_t channels = has_alpha ? 4 : 3;

  std::string out;
  out.resize(pixel_count * channels);

  std::vector<double> component_values(static_cast<std::size_t>(components));
  std::vector<std::uint32_t> raw_samples(static_cast<std::size_t>(components));
  std::size_t out_index = 0;
  std::size_t pixel_index = 0;
  for (std::int32_t y = 0; y < height; ++y) {
    BitReader reader(samples, static_cast<std::size_t>(y) * row_bytes);
    for (std::int32_t x = 0; x < width; ++x) {
      for (std::int32_t j = 0; j < components; ++j) {
        const std::uint32_t sample = reader.read(bits_per_component);
        const auto k = static_cast<std::size_t>(j);
        raw_samples[k] = sample;
        if (decode.size() >= 2 * (k + 1)) {
          const double d_min = decode[2 * k];
          const double d_max = decode[2 * k + 1];
          component_values[k] = d_min + sample * (d_max - d_min) / max_sample;
        } else if (indexed) {
          // Default Indexed /Decode is [0, 2^bpc-1]: the sample is the palette
          // index, which `to_rgb` looks up directly (8.6.6.3).
          component_values[k] = sample;
        } else {
          component_values[k] = static_cast<double>(sample) / max_sample;
        }
      }
      const std::array<double, 3> pixel = color_space.to_rgb(component_values);
      out[out_index++] = static_cast<char>(to_byte(pixel[0]));
      out[out_index++] = static_cast<char>(to_byte(pixel[1]));
      out[out_index++] = static_cast<char>(to_byte(pixel[2]));
      if (has_alpha) {
        std::uint8_t a =
            alpha.size() == pixel_count ? alpha[pixel_index] : 0xFF;
        if (has_color_key) {
          bool keyed = true;
          for (std::int32_t j = 0; j < components && keyed; ++j) {
            const auto k = static_cast<std::size_t>(j);
            if (raw_samples[k] < color_key[2 * k] ||
                raw_samples[k] > color_key[2 * k + 1]) {
              keyed = false;
            }
          }
          if (keyed) {
            a = 0;
          }
        }
        out[out_index++] = static_cast<char>(a);
      }
      ++pixel_index;
    }
  }

  return write_png(out, width, height, has_alpha ? 4 : 3);
}

std::vector<std::uint8_t> pdf::decode_mask_alpha(
    const std::string &samples, const std::int32_t width,
    const std::int32_t height, const std::int32_t bits_per_component,
    const std::vector<double> &decode, const bool stencil,
    const std::int32_t base_width, const std::int32_t base_height) {
  if (width <= 0 || height <= 0 || base_width <= 0 || base_height <= 0 ||
      bits_per_component <= 0 || bits_per_component > 16) {
    return {};
  }
  const std::uint32_t max_sample =
      (1u << static_cast<std::uint32_t>(bits_per_component)) - 1u;
  const std::size_t row_bytes =
      (static_cast<std::size_t>(width) *
           static_cast<std::size_t>(bits_per_component) +
       7) /
      8;
  // A /Decode of [1 0] inverts the mask sense (8.9.5.4).
  const bool invert = decode.size() >= 2 && decode[0] > decode[1];

  // Decode the mask at its native resolution first, then resample.
  std::vector<std::uint8_t> native(static_cast<std::size_t>(width) *
                                   static_cast<std::size_t>(height));
  std::size_t i = 0;
  for (std::int32_t y = 0; y < height; ++y) {
    BitReader reader(samples, static_cast<std::size_t>(y) * row_bytes);
    for (std::int32_t x = 0; x < width; ++x) {
      const std::uint32_t sample = reader.read(bits_per_component);
      double value = static_cast<double>(sample) / max_sample;
      if (invert) {
        value = 1.0 - value;
      }
      if (stencil) {
        // An explicit stencil /Mask: a decoded 1 masks the base pixel out.
        native[i++] = value >= 0.5 ? 0x00 : 0xFF;
      } else {
        // A soft mask: the grey level is the coverage directly.
        native[i++] = to_byte(value);
      }
    }
  }

  if (width == base_width && height == base_height) {
    return native;
  }
  std::vector<std::uint8_t> out(static_cast<std::size_t>(base_width) *
                                static_cast<std::size_t>(base_height));
  std::size_t o = 0;
  for (std::int32_t by = 0; by < base_height; ++by) {
    const std::int32_t my = std::min(height - 1, by * height / base_height);
    for (std::int32_t bx = 0; bx < base_width; ++bx) {
      const std::int32_t mx = std::min(width - 1, bx * width / base_width);
      out[o++] = native[static_cast<std::size_t>(my) * width + mx];
    }
  }
  return out;
}

std::string pdf::encode_stencil_png(const std::string &samples,
                                    const std::int32_t width,
                                    const std::int32_t height,
                                    const std::array<double, 3> &color,
                                    const std::vector<double> &decode) {
  if (width <= 0 || height <= 0) {
    return {};
  }
  // 1 bpc, one component; rows byte-aligned (8.9.5.2).
  const std::size_t row_bytes = (static_cast<std::size_t>(width) + 7) / 8;
  // Default /Decode is [0 1]: a sample of 0 paints, 1 is transparent. A /Decode
  // of [1 0] swaps that — paint when the decoded value rounds to 0.
  const bool invert = decode.size() >= 2 && decode[0] > decode[1];

  const std::uint8_t r = to_byte(color[0]);
  const std::uint8_t g = to_byte(color[1]);
  const std::uint8_t b = to_byte(color[2]);

  std::string rgba;
  rgba.resize(static_cast<std::size_t>(width) *
              static_cast<std::size_t>(height) * 4);
  std::size_t out_index = 0;
  for (std::int32_t y = 0; y < height; ++y) {
    BitReader reader(samples, static_cast<std::size_t>(y) * row_bytes);
    for (std::int32_t x = 0; x < width; ++x) {
      const bool one = reader.read(1) != 0;
      const bool paint = invert ? one : !one;
      rgba[out_index++] = static_cast<char>(r);
      rgba[out_index++] = static_cast<char>(g);
      rgba[out_index++] = static_cast<char>(b);
      rgba[out_index++] = static_cast<char>(paint ? 0xFF : 0x00);
    }
  }
  return write_png(rgba, width, height, 4);
}

std::optional<pdf::EncodedImage> pdf::encode_image(
    std::string raw, const Object &filter, const Object &decode_parms,
    const std::int32_t width, const std::int32_t height,
    const std::int32_t bits_per_component, const ColorSpaceDef *color_space,
    const std::vector<double> &decode_array,
    const std::vector<std::uint8_t> &alpha,
    const std::vector<double> &color_key) {
  const std::optional<std::string> terminal = terminal_image_codec(filter);

  if (terminal == "DCTDecode") {
    // A JPEG the browser decodes itself: hand back its bytes undecoded.
    DecodeResult result = decode(filter, decode_parms, std::move(raw));
    if (result.stopped_at_filter == "DCTDecode") {
      return EncodedImage{std::move(result.data), "image/jpeg"};
    }
    return std::nullopt;
  }
  if (terminal.has_value()) {
    return std::nullopt; // JPX/CCITT/JBIG2: not yet a pass-through
  }

  // A fully decodable raster: decode, assemble samples and PNG-encode.
  if (color_space == nullptr) {
    return std::nullopt;
  }
  DecodeResult result = decode(filter, decode_parms, std::move(raw));
  if (result.stopped_at_filter.has_value()) {
    return std::nullopt;
  }
  std::string png =
      encode_image_png(result.data, width, height, bits_per_component,
                       *color_space, decode_array, alpha, color_key);
  if (png.empty()) {
    return std::nullopt;
  }
  return EncodedImage{std::move(png), "image/png"};
}

} // namespace odr::internal
