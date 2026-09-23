#include <odr/internal/png/png_util.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <array>
#include <string_view>

namespace odr::internal {

namespace {

/// Appends a png chunk: length, four-byte type, data, crc over type+data.
void write_chunk(std::string &out, const std::string_view type,
                 const std::string &data) {
  util::byte_string::put_u32_be(out, static_cast<std::uint32_t>(data.size()));
  const std::size_t crc_start = out.size();
  out.append(type);
  out.append(data);
  util::byte_string::put_u32_be(
      out, crypto::util::crc32(std::string_view(out).substr(crc_start)));
}

/// Signature, `IHDR`, an optional `PLTE`, one `IDAT` and `IEND`. Every
/// scanline is filtered as `None` (PNG 9.2), no interlacing.
std::string assemble(const std::string &rows, const std::size_t stride,
                     const std::int32_t width, const std::int32_t height,
                     const std::int32_t bit_depth,
                     const std::int32_t colour_type,
                     const std::string &palette) {
  std::string raw;
  raw.reserve((stride + 1) * static_cast<std::size_t>(height));
  for (std::int32_t y = 0; y < height; ++y) {
    raw.push_back(0);
    raw.append(rows, static_cast<std::size_t>(y) * stride, stride);
  }

  static constexpr std::array<char, 8> signature = {
      static_cast<char>(0x89), 'P', 'N', 'G', '\r', '\n',
      static_cast<char>(0x1A), '\n'};
  std::string out;
  out.append(signature.data(), signature.size());

  std::string ihdr;
  util::byte_string::put_u32_be(ihdr, static_cast<std::uint32_t>(width));
  util::byte_string::put_u32_be(ihdr, static_cast<std::uint32_t>(height));
  ihdr.push_back(static_cast<char>(bit_depth));
  ihdr.push_back(static_cast<char>(colour_type));
  ihdr.push_back(0); // compression: deflate
  ihdr.push_back(0); // filter method: adaptive
  ihdr.push_back(0); // interlace: none
  write_chunk(out, "IHDR", ihdr);
  if (!palette.empty()) {
    write_chunk(out, "PLTE", palette);
  }
  write_chunk(out, "IDAT", crypto::util::zlib_deflate(raw));
  write_chunk(out, "IEND", "");
  return out;
}

} // namespace

std::string png::write(const std::string &pixels, const std::int32_t width,
                       const std::int32_t height, const std::int32_t channels) {
  if (width <= 0 || height <= 0 || (channels != 3 && channels != 4)) {
    return {};
  }
  const auto stride =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
  if (pixels.size() < stride * static_cast<std::size_t>(height)) {
    return {};
  }
  // colour type: 2 = truecolour (rgb), 6 = truecolour with alpha (rgba)
  return assemble(pixels, stride, width, height, 8, channels == 4 ? 6 : 2, "");
}

std::string png::write_indexed(const std::string &rows,
                               const std::int32_t width,
                               const std::int32_t height,
                               const std::int32_t bit_depth,
                               const std::string &palette) {
  if (width <= 0 || height <= 0 ||
      (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 && bit_depth != 8) ||
      palette.empty() || palette.size() % 3 != 0 ||
      palette.size() / 3 > (std::size_t{1} << bit_depth)) {
    return {};
  }
  const std::size_t stride =
      (static_cast<std::size_t>(width) * static_cast<std::size_t>(bit_depth) +
       7) /
      8;
  if (rows.size() < stride * static_cast<std::size_t>(height)) {
    return {};
  }
  return assemble(rows, stride, width, height, bit_depth, 3, palette);
}

} // namespace odr::internal
