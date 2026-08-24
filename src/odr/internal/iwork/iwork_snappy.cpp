#include <odr/internal/iwork/iwork_snappy.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace odr::internal {

namespace {

/// Reads @p size little-endian bytes as an unsigned integer.
std::uint32_t read_little_endian(const std::string_view in,
                                 const std::size_t position,
                                 const std::size_t size) {
  if (position + size > in.size()) {
    throw std::runtime_error("iwork: snappy block ends mid-tag");
  }

  std::uint32_t result = 0;
  for (std::size_t i = 0; i < size; ++i) {
    result |=
        static_cast<std::uint32_t>(static_cast<std::uint8_t>(in[position + i]))
        << (8 * i);
  }
  return result;
}

/// Reads the block's uncompressed length and advances @p position past it.
std::uint32_t read_uncompressed_length(const std::string_view in,
                                       std::size_t &position) {
  std::uint32_t result = 0;
  for (std::uint32_t shift = 0; shift <= 28; shift += 7) {
    if (position >= in.size()) {
      throw std::runtime_error(
          "iwork: snappy length varint does not terminate");
    }
    const auto byte = static_cast<std::uint8_t>(in[position++]);
    result |= static_cast<std::uint32_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      return result;
    }
  }
  throw std::runtime_error("iwork: snappy length varint does not terminate");
}

/// The most a block can emit per compressed byte it holds: the tag that
/// writes the most for its size is a two-byte-offset copy, three bytes for at
/// most 64. Generous headroom over that, so no real block trips it.
constexpr std::size_t max_expansion = 64;

} // namespace

std::string iwork::snappy_decompress_block(const std::string_view compressed) {
  std::size_t position = 0;
  const std::uint32_t uncompressed_length =
      read_uncompressed_length(compressed, position);

  std::string result;
  // the declared length is the file's word, not a fact, so the allocation is
  // capped by what the block could hold rather than by what it claims
  result.reserve(std::min<std::size_t>(uncompressed_length,
                                       max_expansion * compressed.size()));

  // nothing is appended before it is known to fit, so a block never
  // materialises more than it declared
  const auto check_fits = [&](const std::size_t length) {
    if (length > uncompressed_length - result.size()) {
      throw std::runtime_error("iwork: snappy block writes past its length");
    }
  };

  while (position < compressed.size()) {
    const auto tag = static_cast<std::uint8_t>(compressed[position++]);

    if ((tag & 0x03) == 0) {
      // literal: the length is in the tag, or in the bytes following it
      std::size_t length = tag >> 2;
      if (length >= 60) {
        const std::size_t length_size = length - 59;
        length = read_little_endian(compressed, position, length_size);
        position += length_size;
      }
      ++length;

      if (position + length > compressed.size()) {
        throw std::runtime_error("iwork: snappy literal runs past the block");
      }
      check_fits(length);
      result.append(compressed, position, length);
      position += length;
      continue;
    }

    // copy: a length and a back reference into what has been written already
    std::size_t length = 0;
    std::size_t offset = 0;
    if ((tag & 0x03) == 1) {
      length = 4 + ((tag >> 2) & 0x07);
      offset = (static_cast<std::size_t>(tag >> 5) << 8) |
               read_little_endian(compressed, position, 1);
      position += 1;
    } else {
      const std::size_t offset_size = (tag & 0x03) == 2 ? 2 : 4;
      length = (tag >> 2) + 1;
      offset = read_little_endian(compressed, position, offset_size);
      position += offset_size;
    }

    if (offset == 0 || offset > result.size()) {
      throw std::runtime_error("iwork: snappy copy points outside the block");
    }
    check_fits(length);
    // the copy may overlap what it writes, so it runs byte by byte
    for (std::size_t i = 0, from = result.size() - offset; i < length; ++i) {
      result.push_back(result[from + i]);
    }
  }

  if (result.size() != uncompressed_length) {
    throw std::runtime_error("iwork: snappy block does not fill its length");
  }
  return result;
}

std::string iwork::iwa_decompress(const std::string_view framed) {
  std::string result;

  std::size_t position = 0;
  while (position < framed.size()) {
    if (position + 4 > framed.size()) {
      throw std::runtime_error("iwork: iwa block header is cut off");
    }
    if (framed[position] != '\0') {
      throw std::runtime_error("iwork: iwa block header is not zero");
    }
    const std::uint32_t length = read_little_endian(framed, position + 1, 3);
    position += 4;

    if (position + length > framed.size()) {
      throw std::runtime_error("iwork: iwa block runs past the file");
    }
    result += snappy_decompress_block(framed.substr(position, length));
    position += length;
  }

  return result;
}

} // namespace odr::internal
