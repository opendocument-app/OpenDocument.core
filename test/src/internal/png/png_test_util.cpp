#include "png_test_util.hpp"

#include <odr/internal/crypto/crypto_util.hpp>

#include <stdexcept>

namespace odr::test {

namespace {

void require(const bool valid, const char *message) {
  if (!valid) {
    throw std::runtime_error(message);
  }
}

std::uint32_t be32(const std::string_view data, const std::size_t offset) {
  require(offset <= data.size() && data.size() - offset >= 4,
          "truncated PNG integer");
  std::uint32_t value = 0;
  for (const char byte : data.substr(offset, 4)) {
    value = (value << 8) | static_cast<std::uint8_t>(byte);
  }
  return value;
}

} // namespace

std::string png::bytes(const std::initializer_list<std::uint8_t> values) {
  return {values.begin(), values.end()};
}

std::string png::DecodedPng::pixel(const std::uint32_t x,
                                   const std::uint32_t y) const {
  require(x < width && y < height, "PNG pixel outside image");
  const std::size_t channels = colour_type == 6 ? 4 : 3;
  return pixels.substr((static_cast<std::size_t>(y) * width + x) * channels,
                       channels);
}

png::DecodedPng png::decode_png(const std::string_view data) {
  require(data.starts_with("\x89PNG\r\n\x1a\n"), "invalid PNG signature");
  DecodedPng result;
  std::string idat;
  std::string_view palette;
  std::size_t at = 8;
  while (true) {
    require(at <= data.size() && data.size() - at >= 12, "truncated PNG chunk");
    const std::size_t length = be32(data, at);
    require(length <= data.size() - at - 12 && length <= INT32_MAX,
            "invalid PNG chunk length");
    const auto type = data.substr(at + 4, 4);
    const auto payload = data.substr(at + 8, length);
    require(be32(data, at + 8 + length) ==
                internal::crypto::util::crc32(data.substr(at + 4, length + 4)),
            "invalid PNG chunk CRC");
    require(result.width != 0 || type == "IHDR", "missing PNG header");
    if (type == "IHDR") {
      require(at == 8 && length == 13, "invalid PNG header");
      result.width = be32(payload, 0);
      result.height = be32(payload, 4);
      result.bit_depth = static_cast<std::uint8_t>(payload[8]);
      result.colour_type = static_cast<std::uint8_t>(payload[9]);
      require(result.width != 0 && result.width <= INT32_MAX &&
                  result.height != 0 && result.height <= INT32_MAX,
              "invalid PNG dimensions");
      const bool indexed = result.colour_type == 3;
      require(indexed ? (result.bit_depth == 1 || result.bit_depth == 2 ||
                         result.bit_depth == 4 || result.bit_depth == 8)
                      : ((result.colour_type == 2 || result.colour_type == 6) &&
                         result.bit_depth == 8),
              "unsupported PNG pixel format");
      require(payload[10] == 0 && payload[11] == 0 && payload[12] == 0,
              "unsupported PNG compression, filtering or interlace");
    } else if (type == "PLTE") {
      require(length != 0 && length <= 768 && length % 3 == 0,
              "invalid PNG palette");
      palette = payload;
    } else if (type == "IDAT") {
      ++result.idat_chunks;
      idat += payload;
    } else if (type == "IEND") {
      require(length == 0 && at + 12 == data.size(), "invalid PNG end chunk");
      break;
    }
    at += length + 12;
  }

  const bool indexed = result.colour_type == 3;
  const std::uint64_t channels = result.colour_type == 6 ? 4 : 3;
  const std::uint64_t bits = indexed ? result.bit_depth : channels * 8;
  const std::uint64_t stride = (result.width * bits + 7) / 8;
  const std::string raw = internal::crypto::util::zlib_inflate(idat);
  require(result.height <= raw.size() / (stride + 1) &&
              (stride + 1) * result.height == raw.size(),
          "invalid PNG pixel data length");
  const std::uint64_t pixel_bytes = result.width * channels * result.height;
  require(pixel_bytes <= result.pixels.max_size(), "PNG pixel data too large");
  result.pixels.reserve(static_cast<std::size_t>(pixel_bytes));
  for (std::uint32_t y = 0; y < result.height; ++y) {
    const auto row = static_cast<std::size_t>(y * (stride + 1));
    require(raw[row] == 0, "unsupported PNG row filter");
    if (!indexed) {
      result.pixels.append(raw, row + 1, static_cast<std::size_t>(stride));
      continue;
    }
    for (std::uint32_t x = 0; x < result.width; ++x) {
      const std::uint64_t bit = x * bits;
      const auto byte = static_cast<std::uint8_t>(raw[row + 1 + bit / 8]);
      const std::size_t index =
          (byte >> (8 - bits - bit % 8)) & ((1u << bits) - 1);
      require(index < palette.size() / 3, "PNG palette index out of bounds");
      result.pixels.append(palette.substr(index * 3, 3));
    }
  }
  return result;
}

} // namespace odr::test
