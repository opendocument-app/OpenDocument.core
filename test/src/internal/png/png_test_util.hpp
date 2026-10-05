#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace odr::test::png {

struct DecodedPng {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint8_t bit_depth{};
  std::uint8_t colour_type{};
  std::size_t idat_chunks{};
  std::string pixels;

  [[nodiscard]] std::string pixel(std::uint32_t x, std::uint32_t y) const;
};

/// Decode the encoder's unfiltered RGB, RGBA or indexed output; throw on
/// corruption.
DecodedPng decode_png(std::string_view data);
std::string bytes(std::initializer_list<std::uint8_t> values);

} // namespace odr::test::png
