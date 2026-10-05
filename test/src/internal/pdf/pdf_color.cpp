#include <odr/internal/pdf/pdf_color.hpp>

#include <odr/internal/pdf/pdf_object.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {

ColorSpaceContext context() {
  ColorSpaceContext ctx;
  ctx.resolve = [](const Object &object) { return object; };
  ctx.load_stream = [](const Object &) { return std::string{}; };
  ctx.named = nullptr;
  return ctx;
}

Object reals(std::initializer_list<double> values) {
  std::vector<Object> holder;
  for (const double value : values) {
    holder.emplace_back(Real{value});
  }
  return Object(Array(std::move(holder)));
}

ColorSpaceDef device(const ColorSpaceKind kind, const std::int32_t components) {
  ColorSpaceDef def;
  def.kind = kind;
  def.components = components;
  return def;
}

/// `ColorSpaceDef::to_rgb` over a braced component list, which its `std::span`
/// does not take.
std::array<double, 3> to_rgb(const ColorSpaceDef &def,
                             const std::vector<double> &components) {
  return def.to_rgb(components);
}

} // namespace

// The device spaces convert as expected. CMYK follows Adobe's transform, so no
// ink is white, full key is a dark neutral rather than `#000`, and full cyan is
// a process cyan.
TEST(PdfColor, device_spaces) {
  EXPECT_EQ(to_rgb(device(ColorSpaceKind::device_gray, 1), {0.5}),
            (std::array<double, 3>{0.5, 0.5, 0.5}));
  EXPECT_EQ(to_rgb(device(ColorSpaceKind::device_rgb, 3), {0.2, 0.4, 0.6}),
            (std::array<double, 3>{0.2, 0.4, 0.6}));
  EXPECT_EQ(to_rgb(device(ColorSpaceKind::device_cmyk, 4), {0, 0, 0, 0}),
            (std::array<double, 3>{1, 1, 1}));
  const std::array<double, 3> key =
      to_rgb(device(ColorSpaceKind::device_cmyk, 4), {0, 0, 0, 1});
  EXPECT_NEAR(key[0], 0.17, 0.01);
  EXPECT_NEAR(key[1], 0.18, 0.01);
  EXPECT_NEAR(key[2], 0.21, 0.01);
  const std::array<double, 3> cyan =
      to_rgb(device(ColorSpaceKind::device_cmyk, 4), {1, 0, 0, 0});
  EXPECT_NEAR(cyan[0], 0.0, 0.01);
  EXPECT_NEAR(cyan[1], 0.72, 0.01);
  EXPECT_NEAR(cyan[2], 0.95, 0.01);
}

// L*a*b* maps the lightness extremes to white and black under the default
// (D65) white point.
TEST(PdfColor, lab_extremes) {
  ColorSpaceDef lab = device(ColorSpaceKind::lab, 3);
  const std::array<double, 3> white = to_rgb(lab, {100, 0, 0});
  EXPECT_NEAR(white[0], 1.0, 0.02);
  EXPECT_NEAR(white[1], 1.0, 0.02);
  EXPECT_NEAR(white[2], 1.0, 0.02);
  const std::array<double, 3> black = to_rgb(lab, {0, 0, 0});
  EXPECT_NEAR(black[0], 0.0, 0.02);
  EXPECT_NEAR(black[1], 0.0, 0.02);
  EXPECT_NEAR(black[2], 0.0, 0.02);
}

// ICCBased approximates by its component count when no engine is present:
// N = 3 behaves as DeviceRGB.
TEST(PdfColor, iccbased_by_component_count) {
  Dictionary stream_dict;
  stream_dict["N"] = Object(Integer{3});
  std::vector<Object> array{Object(Name{"ICCBased"}), Object(stream_dict)};

  const auto def =
      parse_color_space(Object(Array(std::move(array))), context());
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->kind, ColorSpaceKind::icc_based);
  EXPECT_EQ(def->components, 3);
  EXPECT_EQ(to_rgb(*def, {0.2, 0.4, 0.6}),
            (std::array<double, 3>{0.2, 0.4, 0.6}));
}

// Indexed looks an index up in the palette and converts through the base space.
TEST(PdfColor, indexed_palette) {
  // hival 1, base DeviceRGB, palette = [255,0,0, 0,255,0].
  const std::string palette("\xff\x00\x00\x00\xff\x00", 6);
  std::vector<Object> array{Object(Name{"Indexed"}), Object(Name{"DeviceRGB"}),
                            Object(Integer{1}),
                            Object(StandardString(palette))};

  const auto def =
      parse_color_space(Object(Array(std::move(array))), context());
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->kind, ColorSpaceKind::indexed);
  EXPECT_EQ(to_rgb(*def, {0}), (std::array<double, 3>{1, 0, 0}));
  EXPECT_EQ(to_rgb(*def, {1}), (std::array<double, 3>{0, 1, 0}));
  for (const double value : {-1e100, 0.49}) {
    EXPECT_EQ(to_rgb(*def, {value}), (std::array<double, 3>{1, 0, 0}));
  }
  for (const double value : {0.5, 1e100}) {
    EXPECT_EQ(to_rgb(*def, {value}), (std::array<double, 3>{0, 1, 0}));
  }
}

// Separation samples its tint transform, then converts through the alternate.
TEST(PdfColor, separation_tint_transform) {
  // tint: type 2, C0 = white, C1 = red, N = 1 -> tint(t) = (1, 1-t, 1-t).
  Dictionary tint;
  tint["FunctionType"] = Object(Integer{2});
  tint["Domain"] = reals({0, 1});
  tint["C0"] = reals({1, 1, 1});
  tint["C1"] = reals({1, 0, 0});
  tint["N"] = Object(Real{1});

  std::vector<Object> array{Object(Name{"Separation"}), Object(Name{"Spot"}),
                            Object(Name{"DeviceRGB"}), Object(tint)};

  const auto def =
      parse_color_space(Object(Array(std::move(array))), context());
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->kind, ColorSpaceKind::separation);
  EXPECT_EQ(def->components, 1);
  // full tint -> C1 = red
  EXPECT_EQ(to_rgb(*def, {1.0}), (std::array<double, 3>{1, 0, 0}));
  // half tint -> (1, 0.5, 0.5)
  const std::array<double, 3> half = to_rgb(*def, {0.5});
  EXPECT_NEAR(half[0], 1.0, 1e-9);
  EXPECT_NEAR(half[1], 0.5, 1e-9);
  EXPECT_NEAR(half[2], 0.5, 1e-9);
}

// A tint transform without a finite answer paints as the ink approximation
// rather than aborting the page.
TEST(PdfColor, separation_non_finite_tint_approximates_ink) {
  Dictionary tint;
  tint["FunctionType"] = Object(Integer{2});
  tint["Domain"] = reals({0, 2});
  tint["C0"] = reals({0});
  tint["C1"] = reals({1});
  tint["N"] = Object(Real{2048}); // 2^2048 overflows

  std::vector<Object> array{Object(Name{"Separation"}), Object(Name{"Spot"}),
                            Object(Name{"DeviceGray"}), Object(tint)};
  const auto def =
      parse_color_space(Object(Array(std::move(array))), context());
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(to_rgb(*def, {2.0}), (std::array<double, 3>{0, 0, 0}));
}

// A colour space's initial component values (ISO 32000-1 8.6.3): zero for the
// device families, full tint for Separation/DeviceN.
TEST(PdfColor, initial_components) {
  EXPECT_EQ(device(ColorSpaceKind::device_rgb, 3).initial_components(),
            (std::vector<double>{0, 0, 0}));
  ColorSpaceDef sep = device(ColorSpaceKind::separation, 1);
  EXPECT_EQ(sep.initial_components(), (std::vector<double>{1.0}));
  // DeviceCMYK starts at black {0, 0, 0, 1}, not the all-zero (white) default,
  // so a resource alias to /DeviceCMYK matches a direct /DeviceCMYK selection.
  const ColorSpaceDef cmyk = device(ColorSpaceKind::device_cmyk, 4);
  EXPECT_EQ(cmyk.initial_components(), (std::vector<double>{0, 0, 0, 1}));
  EXPECT_EQ(cmyk.to_rgb(cmyk.initial_components()), cmyk_to_rgb(0, 0, 0, 1));
}

// A name resolves to the matching device space.
TEST(PdfColor, name_resolves_device_space) {
  const auto def = parse_color_space(Object(Name{"DeviceCMYK"}), context());
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->kind, ColorSpaceKind::device_cmyk);
  EXPECT_EQ(def->components, 4);
}

TEST(PdfColor, invalid_component_counts_and_palettes_are_rejected) {
  for (const Integer count : {Integer{-1}, Integer{0}, Integer{2}, Integer{5},
                              std::numeric_limits<Integer>::max()}) {
    Dictionary profile;
    profile["N"] = Object(count);
    EXPECT_EQ(parse_color_space(
                  Object(Array({Object(Name{"ICCBased"}), Object(profile)})),
                  context()),
              nullptr)
        << count;
  }
  for (const Integer hival :
       {Integer{-1}, Integer{256}, std::numeric_limits<Integer>::max()}) {
    EXPECT_EQ(
        parse_color_space(
            Object(Array({Object(Name{"Indexed"}), Object(Name{"DeviceGray"}),
                          Object(hival),
                          Object(StandardString(std::string(256, '\0')))})),
            context()),
        nullptr)
        << hival;
  }
  EXPECT_EQ(
      parse_color_space(
          Object(Array({Object(Name{"Indexed"}), Object(Name{"DeviceRGB"}),
                        Object(Integer{1}), Object(StandardString("short"))})),
          context()),
      nullptr);
}

TEST(PdfColor, device_n_requires_nonempty_colorant_names) {
  for (const Object &names : {Object(Name{"Spot"}), Object(Array{}),
                              Object(Array({Object(Integer{1})}))}) {
    EXPECT_EQ(
        parse_color_space(Object(Array({Object(Name{"DeviceN"}), names,
                                        Object(Name{"DeviceRGB"}), Object{}})),
                          context()),
        nullptr);
  }
}

TEST(PdfColor, recursive_alternates_are_rejected) {
  Dictionary profile;
  profile["N"] = Object(Integer{3});
  profile["Alternate"] = Object(ObjectReference{1, 0});
  const Object cyclic(Array({Object(Name{"ICCBased"}), Object(profile)}));
  ColorSpaceContext ctx = context();
  ctx.resolve = [&](const Object &object) {
    return object.is_reference() ? cyclic : object;
  };
  EXPECT_EQ(parse_color_space(cyclic, ctx), nullptr);
  ASSERT_NE(parse_color_space(Object(Name{"DeviceRGB"}), ctx), nullptr);
}
