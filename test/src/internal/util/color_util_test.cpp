#include <odr/internal/util/color_util.hpp>

#include <odr/style.hpp>

#include <array>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal::util::color;

namespace {

constexpr std::array<std::uint32_t, 6> samples = {0x000000, 0xffffff, 0x4472c4,
                                                  0xdee6ef, 0x7f7f7f, 0xc00000};

} // namespace

TEST(ColorUtil, to_byte_clamps_and_rounds) {
  EXPECT_EQ(to_byte(-0.5), 0);
  EXPECT_EQ(to_byte(0.5), 128);
  EXPECT_EQ(to_byte(1.5), 255);
}

TEST(ColorUtil, srgb_transfer_round_trips) {
  EXPECT_NEAR(srgb_to_linear(0.5), 0.214041, 1e-6);
  for (const double c : {0.0, 0.01, 0.5, 1.0}) {
    EXPECT_NEAR(linear_to_srgb(srgb_to_linear(c)), c, 1e-12);
  }
}

TEST(Hsl, from_color) {
  const Hsl hsl = Hsl::from_color(0x4472c4_rgb);
  EXPECT_NEAR(hsl.hue * 360, 218.4, 0.1);
  EXPECT_NEAR(hsl.saturation, 0.521, 1e-3);
  EXPECT_NEAR(hsl.lightness, 0.518, 1e-3);

  EXPECT_EQ(Hsl::from_color(0x7f7f7f_rgb).saturation, 0);
}

TEST(Hsl, round_trips) {
  for (const std::uint32_t rgb : samples) {
    EXPECT_EQ(Hsl::from_color(Color::from_rgb(rgb)).to_color(255).rgb(), rgb);
  }
}

TEST(Oklab, white_has_no_chroma) {
  const Oklab white = Oklab::from_color(0xffffff_rgb);
  EXPECT_NEAR(white.l, 1, 1e-6);
  EXPECT_NEAR(white.a, 0, 1e-6);
  EXPECT_NEAR(white.b, 0, 1e-6);
}

TEST(Oklab, round_trips) {
  for (const std::uint32_t rgb : samples) {
    const std::array<double, 3> linear =
        Oklab::from_color(Color::from_rgb(rgb)).to_linear_rgb();
    const Color color(to_byte(linear_to_srgb(linear[0])),
                      to_byte(linear_to_srgb(linear[1])),
                      to_byte(linear_to_srgb(linear[2])));
    EXPECT_EQ(color.rgb(), rgb);
  }
}
