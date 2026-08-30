#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/svm/svm_format.hpp>
#include <odr/internal/svm/svm_to_svg.hpp>

#include <test_util.hpp>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::test;

namespace {

/// Builds a metafile byte by byte, as `SvmReader` reads it back.
class SvmBuilder final {
public:
  SvmBuilder &u8(const std::uint8_t value) {
    m_data.push_back(static_cast<char>(value));
    return *this;
  }

  SvmBuilder &u16(const std::uint16_t value) {
    return u8(value & 0xff).u8(value >> 8 & 0xff);
  }

  SvmBuilder &u32(const std::uint32_t value) {
    return u16(value & 0xffff).u16(value >> 16 & 0xffff);
  }

  SvmBuilder &i32(const std::int32_t value) {
    return u32(static_cast<std::uint32_t>(value));
  }

  SvmBuilder &point(const std::int32_t x, const std::int32_t y) {
    return i32(x).i32(y);
  }

  SvmBuilder &rectangle(const std::int32_t left, const std::int32_t top,
                        const std::int32_t right, const std::int32_t bottom) {
    return i32(left).i32(top).i32(right).i32(bottom);
  }

  SvmBuilder &
  polygon(const std::vector<std::pair<std::int32_t, std::int32_t>> &points) {
    u16(static_cast<std::uint16_t>(points.size()));
    for (const auto &[x, y] : points) {
      point(x, y);
    }
    return *this;
  }

  /// The font the text actions below draw with, at @p size.
  SvmBuilder &
  font(const std::string &family, const std::int32_t size,
       const std::uint16_t weight = 0, const std::uint16_t underline = 0,
       const std::uint16_t strikeout = 0, const std::uint16_t italic = 0,
       const std::uint16_t orientation = 0, const std::uint16_t charset = 11) {
    return action(svm::META_FONT_ACTION)
        .begin()
        .ascii_string(family)
        .ascii_string("")
        .point(0, size)
        .u16(charset)
        .u16(0) // family
        .u16(0) // pitch
        .u16(weight)
        .u16(underline)
        .u16(strikeout)
        .u16(italic)
        .u16(0) // language
        .u16(0) // width
        .u16(orientation)
        .u8(0) // wordline
        .u8(0) // outline
        .u8(0) // shadow
        .u8(0) // kerning
        .end()
        .end();
  }

  SvmBuilder &ucs2_string(const std::u16string &value) {
    u32(static_cast<std::uint32_t>(value.size()));
    for (const char16_t c : value) {
      u16(static_cast<std::uint16_t>(c));
    }
    return *this;
  }

  /// A `TEXT` action of @p text at @p point, drawing the run @p offset /
  /// @p length names.
  SvmBuilder &text(const std::int32_t x, const std::int32_t y,
                   const std::string &value, const std::uint16_t offset = 0,
                   const std::uint16_t length = 0xffff) {
    return action(svm::META_TEXT_ACTION)
        .point(x, y)
        .ascii_string(value)
        .u16(offset)
        .u16(length)
        .end();
  }

  /// A colour *inside* an object, which is not the plain `uint32` an action's
  /// own colour is: a name id, and three 16-bit channels behind the user one.
  SvmBuilder &object_color(const std::uint32_t rgb) {
    return u16(0x8000)
        .u16(static_cast<std::uint16_t>((rgb >> 16 & 0xff) << 8))
        .u16(static_cast<std::uint16_t>((rgb >> 8 & 0xff) << 8))
        .u16(static_cast<std::uint16_t>((rgb & 0xff) << 8));
  }

  SvmBuilder &gradient(const std::uint16_t style, const std::uint32_t start,
                       const std::uint32_t end, const std::uint16_t angle = 0) {
    begin().u16(style).object_color(start).object_color(end);
    return u16(angle)
        .u16(0)   // border
        .u16(50)  // offset x
        .u16(50)  // offset y
        .u16(100) // start intensity
        .u16(100) // end intensity
        .u16(0)   // step count
        .end();
  }

  /// A poly-polygon of one rectangle.
  SvmBuilder &poly_rectangle(const std::int32_t left, const std::int32_t top,
                             const std::int32_t right,
                             const std::int32_t bottom) {
    return u16(1).polygon(
        {{left, top}, {right, top}, {right, bottom}, {left, bottom}});
  }

  /// A region of one band, the shape `ReadRegion` reads a rectangle as.
  SvmBuilder &region(const std::int32_t left, const std::int32_t top,
                     const std::int32_t right, const std::int32_t bottom) {
    return begin(2)
        .u16(1) // content version
        .u16(2) // type: rectangle
        .u16(0) // band header
        .i32(top)
        .i32(bottom)
        .u16(1) // separation
        .i32(left)
        .i32(right)
        .u16(2) // end
        .u8(0)  // no poly-polygon
        .end();
  }

  /// A 24-bit uncompressed dib with the `BITMAPFILEHEADER` a metafile stores
  /// it behind. @p pixels is @p width * @p height bgr triples, top row first;
  /// the rows go out bottom-up and padded, as a dib holds them.
  SvmBuilder &dib(const std::int32_t width, const std::int32_t height,
                  const std::string &pixels) {
    const std::size_t stride = ((width * 24 + 31) / 32) * 4;
    const auto image_size = static_cast<std::uint32_t>(stride * height);

    u16(0x4d42);         // "BM"
    u32(54 + image_size) // bfSize
        .u16(0)          // reserved
        .u16(0)          // reserved
        .u32(54);        // bfOffBits
    u32(40)              // header size
        .i32(width)
        .i32(height)
        .u16(1)  // planes
        .u16(24) // bit count
        .u32(0)  // compression
        .u32(image_size)
        .i32(0) // pixels per metre x
        .i32(0) // pixels per metre y
        .u32(0) // colours used
        .u32(0);
    for (std::int32_t y = height - 1; y >= 0; --y) {
      const std::string row =
          pixels.substr(static_cast<std::size_t>(y) * width * 3, width * 3);
      m_data += row;
      m_data.append(stride - row.size(), '\0');
    }
    return *this;
  }

  /// A pascal string, as `read_uint16_prefixed_ascii_string` reads it.
  SvmBuilder &ascii_string(const std::string &value) {
    u16(static_cast<std::uint16_t>(value.size()));
    m_data += value;
    return *this;
  }

  /// Opens a `VersionCompat` whose length is filled in by @ref end.
  SvmBuilder &begin(const std::uint16_t version = 1) {
    u16(version);
    m_open.push_back(m_data.size());
    u32(0);
    return *this;
  }

  SvmBuilder &end() {
    const std::size_t offset = m_open.back();
    m_open.pop_back();
    const std::size_t length = m_data.size() - offset - 4;
    for (std::size_t i = 0; i < 4; ++i) {
      m_data[offset + i] = static_cast<char>(length >> (8 * i) & 0xff);
    }
    return *this;
  }

  /// The action's type and `VersionCompat`; ends with @ref end.
  SvmBuilder &action(const svm::MetaActionType type,
                     const std::uint16_t version = 1) {
    return u16(static_cast<std::uint16_t>(type)).begin(version);
  }

  SvmBuilder &map_mode(const std::int32_t scale_numerator = 1,
                       const std::int32_t scale_denominator = 1) {
    return begin()
        .u16(0)                 // unit
        .point(0, 0)            // origin
        .i32(scale_numerator)   // scale x
        .i32(scale_denominator) //
        .i32(scale_numerator)   // scale y
        .i32(scale_denominator) //
        .u8(0)                  // simple
        .end();
  }

  /// `"VCLMTF"`, the header, and whatever actions follow.
  [[nodiscard]] std::string file(const std::int32_t width = 100,
                                 const std::int32_t height = 100) const {
    SvmBuilder header;
    header.m_data = "VCLMTF";
    header.begin(2)
        .u32(0) // compression mode
        .map_mode()
        .point(width, height)
        .u32(0) // action count
        .u8(0)  // render graphic replacements
        .end();
    return header.m_data + m_data;
  }

private:
  std::string m_data;
  std::vector<std::size_t> m_open;
};

std::size_t count_of(const std::string &haystack, const std::string &needle) {
  std::size_t result = 0;
  for (std::size_t at = haystack.find(needle); at != std::string::npos;
       at = haystack.find(needle, at + 1)) {
    ++result;
  }
  return result;
}

std::string translate(const std::string &data) {
  const svm::SvmFile file(std::make_shared<MemoryFile>(data));
  std::ostringstream out;
  svm::translate_to_svg(file, out, odr::Logger::null());
  return out.str();
}

} // namespace

TEST(SvmFile, open) {
  const svm::SvmFile svm(std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/svm/chart-1.svm")));

  EXPECT_EQ(odr::FileType::starview_metafile, svm.file_type());
}

TEST(SvmToSvg, empty) {
  EXPECT_EQ("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\""
            " viewBox=\"0 0 100 100\" />",
            translate(SvmBuilder().file()));
}

TEST(SvmToSvg, rectangle) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_FILLCOLOR_ACTION)
                                        .u32(0x0000ff)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_RECT_ACTION)
                                        .rectangle(1, 2, 11, 22)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("<rect x=\"1\" y=\"2\" width=\"10\""
                                        " height=\"20\""));
  EXPECT_NE(std::string::npos, svg.find("fill:rgb(0,0,255)"));
}

TEST(SvmToSvg, text) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_TEXT_ACTION)
                                        .point(3, 4)
                                        .ascii_string("hello")
                                        .u16(0)
                                        .u16(5)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("<text x=\"3\" y=\"4\""));
  EXPECT_NE(std::string::npos, svg.find(">hello</text>"));
}

TEST(SvmToSvg, text_is_escaped) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_TEXT_ACTION)
                                        .point(0, 0)
                                        .ascii_string("a & b <c>")
                                        .u16(0)
                                        .u16(9)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find(">a &amp; b &lt;c&gt;</text>"));
}

TEST(SvmToSvg, font_family_is_escaped) {
  const std::string svg = translate(SvmBuilder()
                                        .font("a\"b", 10)
                                        .action(svm::META_TEXT_ACTION)
                                        .point(0, 0)
                                        .ascii_string("x")
                                        .u16(0)
                                        .u16(1)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("font-family:a&quot;b"));
}

TEST(SvmToSvg, unhandled_action_is_skipped) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_ELLIPSE_ACTION)
                                        .rectangle(0, 0, 10, 10)
                                        .end()
                                        .action(svm::META_RECT_ACTION)
                                        .rectangle(1, 1, 2, 2)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("<rect x=\"1\" y=\"1\""));
}

TEST(SvmToSvg, string) {
  const svm::SvmFile svm(std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/svm/table-1.svm")));

  std::stringstream out;
  svm::translate_to_svg(svm, out, odr::Logger::null());

  EXPECT_LT(0, out.str().size());
}

TEST(SvmToSvg, a_shape_is_stroked_and_filled) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_LINECOLOR_ACTION)
                                        .u32(0x00ff00)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_FILLCOLOR_ACTION)
                                        .u32(0x0000ff)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_RECT_ACTION)
                                        .rectangle(0, 0, 10, 10)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("stroke:rgb(0,255,0)"));
  EXPECT_NE(std::string::npos, svg.find("fill:rgb(0,0,255)"));
}

TEST(SvmToSvg, an_unset_colour_draws_nothing) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_LINECOLOR_ACTION)
                                        .u32(0x00ff00)
                                        .u8(0)
                                        .end()
                                        .action(svm::META_RECT_ACTION)
                                        .rectangle(0, 0, 10, 10)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("stroke:none"));
}

TEST(SvmToSvg, pop_restores_what_the_push_saved) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_FILLCOLOR_ACTION)
                                        .u32(0xff0000)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_PUSH_ACTION)
                                        .u16(svm::PUSH_FILLCOLOR)
                                        .end()
                                        .action(svm::META_FILLCOLOR_ACTION)
                                        .u32(0x0000ff)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_POP_ACTION)
                                        .end()
                                        .action(svm::META_RECT_ACTION)
                                        .rectangle(0, 0, 10, 10)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("fill:rgb(255,0,0)"));
}

TEST(SvmToSvg, pop_keeps_what_the_push_did_not_save) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_PUSH_ACTION)
                                        .u16(svm::PUSH_LINECOLOR)
                                        .end()
                                        .action(svm::META_FILLCOLOR_ACTION)
                                        .u32(0x0000ff)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_POP_ACTION)
                                        .end()
                                        .action(svm::META_RECT_ACTION)
                                        .rectangle(0, 0, 10, 10)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("fill:rgb(0,0,255)"));
}

TEST(SvmToSvg, a_poly_polygon_is_one_path) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_POLYPOLYGON_ACTION, 2)
                    .u16(2) // polygons
                    .polygon({{0, 0}, {10, 0}, {10, 10}, {0, 10}})
                    .polygon({{2, 2}, {8, 2}, {8, 8}, {2, 8}})
                    .u16(0) // complex polygons
                    .end()
                    .file());

  EXPECT_EQ(1, count_of(svg, "<path"));
  EXPECT_NE(std::string::npos, svg.find("d=\"M 0,0 L 10,0 10,10 0,10 Z"
                                        " M 2,2 L 8,2 8,8 2,8 Z\""));
  EXPECT_NE(std::string::npos, svg.find("fill-rule:evenodd"));
}

TEST(SvmToSvg, a_poly_line_takes_its_line_info) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_POLYLINE_ACTION, 2)
                                        .polygon({{0, 0}, {10, 10}})
                                        .begin(3) // line info
                                        .u16(2)   // dash
                                        .i32(4)   // width
                                        .u16(1)   // dash count
                                        .i32(6)   // dash length
                                        .u16(0)   // dot count
                                        .i32(0)   // dot length
                                        .i32(3)   // distance
                                        .u16(4)   // join: round
                                        .end()
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("stroke-width:4"));
  EXPECT_NE(std::string::npos, svg.find("stroke-linejoin:round"));
  EXPECT_NE(std::string::npos, svg.find("stroke-dasharray:6,3"));
}

TEST(SvmToSvg, the_font_size_is_scaled) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_MAPMODE_ACTION)
                                        .begin()
                                        .u16(0)      // unit
                                        .point(0, 0) // origin
                                        .i32(1)
                                        .i32(2) // scale x
                                        .i32(1)
                                        .i32(2) // scale y
                                        .u8(0)  // simple
                                        .end()
                                        .end()
                                        .font("Arial", 20)
                                        .action(svm::META_TEXT_ACTION)
                                        .point(0, 0)
                                        .ascii_string("x")
                                        .u16(0)
                                        .u16(1)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("font-size:10"));
}

TEST(SvmToSvg, text_align) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_TEXTALIGN_ACTION)
                                        .u16(svm::ALIGN_BOTTOM)
                                        .end()
                                        .text(0, 0, "x")
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("dominant-baseline:text-after-edge"));
}

TEST(SvmToSvg, text_align_defaults_to_the_top) {
  const std::string svg = translate(SvmBuilder().text(0, 0, "x").file());

  EXPECT_NE(std::string::npos, svg.find("dominant-baseline:text-before-edge"));
}

TEST(SvmToSvg, font_attributes) {
  const std::string svg = translate(
      SvmBuilder()
          .font("Arial", 10, svm::WEIGHT_BOLD, 1, 1, svm::ITALIC_NORMAL)
          .text(0, 0, "x")
          .file());

  EXPECT_NE(std::string::npos, svg.find("font-style:italic"));
  EXPECT_NE(std::string::npos, svg.find("font-weight:700"));
  EXPECT_NE(std::string::npos,
            svg.find("text-decoration:underline line-through"));
}

TEST(SvmToSvg, font_orientation_rotates_the_text) {
  const std::string svg = translate(
      SvmBuilder().font("Arial", 10, 0, 0, 0, 0, 900).text(5, 7, "x").file());

  EXPECT_NE(std::string::npos, svg.find("transform=\"rotate(-90 5 7)\""));
}

TEST(SvmToSvg, text_array_places_every_character) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_TEXTARRAY_ACTION)
                                        .point(0, 0)
                                        .ascii_string("abc")
                                        .u16(0)
                                        .u16(3)
                                        .u32(3) // dx array
                                        .u32(10)
                                        .u32(20)
                                        .u32(30)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("x=\"0 10 20\""));
}

TEST(SvmToSvg, a_text_array_that_does_not_match_is_dropped) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_TEXTARRAY_ACTION)
                                        .point(4, 0)
                                        .ascii_string("abc")
                                        .u16(0)
                                        .u16(3)
                                        .u32(1) // dx array
                                        .u32(10)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("x=\"4\""));
}

TEST(SvmToSvg, stretch_text_fills_the_width_it_names) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_STRETCHTEXT_ACTION)
                                        .point(0, 0)
                                        .ascii_string("abc")
                                        .u32(120) // width
                                        .u16(0)
                                        .u16(3)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("textLength=\"120\""));
  EXPECT_NE(std::string::npos, svg.find("lengthAdjust=\"spacingAndGlyphs\""));
}

TEST(SvmToSvg, a_version_1_text_action_slices_a_ucs2_run_by_character) {
  const std::string svg =
      translate(SvmBuilder()
                    .font("f", 10, 0, 0, 0, 0, 0, svm::RTL_TEXTENCODING_UCS2)
                    .action(svm::META_TEXT_ACTION)
                    .point(0, 0)
                    .ucs2_string(u"\u00e4bc")
                    .u16(1) // offset
                    .u16(2) // length
                    .end()
                    .file());

  EXPECT_NE(std::string::npos, svg.find(">bc</text>"));
}

TEST(SvmToSvg, text_draws_the_run_it_names) {
  const std::string svg =
      translate(SvmBuilder().text(0, 0, "abcdef", 2, 3).file());

  EXPECT_NE(std::string::npos, svg.find(">cde</text>"));
}

/// A metafile stores a dib behind its file header, so the bytes already are a
/// bmp - and where the pixels can simply be copied out, a png instead, which
/// is what keeps a chart from costing megabytes.
TEST(SvmToSvg, a_bitmap_is_drawn_where_the_action_puts_it) {
  const std::string white_black_red_blue("\xff\xff\xff"
                                         "\x00\x00\x00"
                                         "\x00\x00\xff"
                                         "\xff\x00\x00",
                                         12);
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_BMPSCALE_ACTION)
                                        .dib(2, 2, white_black_red_blue)
                                        .point(10, 20)
                                        .point(30, 40)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos,
            svg.find("<image x=\"10\" y=\"20\" width=\"30\" height=\"40\""));
  EXPECT_NE(std::string::npos, svg.find("href=\"data:image/png;base64,"));
}

/// The mask says where the bitmap does *not* show, in white - and an svg mask
/// keeps what is white, so it goes through a filter that inverts it.
TEST(SvmToSvg, an_unsized_bitmap_is_drawn_at_its_pixel_size) {
  // 16 pixels at 96 dpi is 16 * 2540 / 96 = 423 hundredths of a millimetre;
  // truncating the per-pixel 26.458 to 26 first would say 416
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_BMP_ACTION)
                                        .dib(16, 1, std::string(48, '\x7f'))
                                        .point(0, 0)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("width=\"423\" height=\"26\""));
}

TEST(SvmToSvg, a_bitmap_mask_is_inverted) {
  const std::string pixels(2 * 2 * 3, '\x40');
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_BMPEXSCALE_ACTION)
                    .dib(2, 2, pixels)
                    .u32(0x25091962) // the transparency data behind it
                    .u32(0xacb20201)
                    .u8(2) // a second dib as the mask
                    .dib(2, 2, pixels)
                    .point(0, 0)
                    .point(10, 10)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos, svg.find("<mask id=\"odr-mask-1\""));
  EXPECT_NE(std::string::npos, svg.find("filter=\"url(#odr-invert)\""));
  EXPECT_NE(std::string::npos, svg.find("mask=\"url(#odr-mask-1)\""));
}

/// What a clip covers is a group around everything drawn after it.
TEST(SvmToSvg, a_clip_region_wraps_what_follows) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_ISECTRECTCLIPREGION_ACTION)
                    .rectangle(1, 2, 11, 22)
                    .end()
                    .action(svm::META_RECT_ACTION)
                    .rectangle(0, 0, 100, 100)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos,
            svg.find("<clipPath id=\"odr-clip-1\">"
                     "<path d=\"M 1,2 L 11,2 11,22 1,22 Z\""));
  EXPECT_NE(std::string::npos,
            svg.find("<g clip-path=\"url(#odr-clip-1)\"><rect"));
  EXPECT_NE(std::string::npos, svg.find("</g></svg>"));
}

/// One region intersected into another is one group inside the other, which
/// is how svg intersects two clips.
TEST(SvmToSvg, clips_intersect_by_nesting) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_ISECTRECTCLIPREGION_ACTION)
                    .rectangle(0, 0, 10, 10)
                    .end()
                    .action(svm::META_ISECTREGIONCLIPREGION_ACTION)
                    .region(2, 2, 8, 8)
                    .end()
                    .action(svm::META_RECT_ACTION)
                    .rectangle(0, 0, 100, 100)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos, svg.find("<g clip-path=\"url(#odr-clip-1)\">"
                                        "<clipPath id=\"odr-clip-2\">"));
  EXPECT_NE(std::string::npos, svg.find("M 2,2 L 8,2 8,8 2,8 Z"));
  EXPECT_NE(std::string::npos, svg.find("</g></g></svg>"));
}

/// A pop that restores the clip closes the group the push was drawn in.
TEST(SvmToSvg, a_pop_restores_the_clip) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_PUSH_ACTION)
                    .u16(svm::PUSH_CLIPREGION)
                    .end()
                    .action(svm::META_ISECTRECTCLIPREGION_ACTION)
                    .rectangle(1, 1, 2, 2)
                    .end()
                    .action(svm::META_RECT_ACTION)
                    .rectangle(0, 0, 100, 100)
                    .end()
                    .action(svm::META_POP_ACTION)
                    .end()
                    .action(svm::META_RECT_ACTION)
                    .rectangle(0, 0, 100, 100)
                    .end()
                    .file());

  // the second rectangle is outside the group the first one is in
  EXPECT_NE(std::string::npos, svg.find("/></g><rect"));
}

/// A file sets the drawing area and then intersects the region of the same
/// rectangle; the second is a group that clips nothing.
TEST(SvmToSvg, the_same_clip_twice_is_one_group) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_ISECTRECTCLIPREGION_ACTION)
                    .rectangle(0, 0, 10, 10)
                    .end()
                    .action(svm::META_ISECTREGIONCLIPREGION_ACTION)
                    .region(0, 0, 10, 10)
                    .end()
                    .action(svm::META_RECT_ACTION)
                    .rectangle(0, 0, 100, 100)
                    .end()
                    .file());

  EXPECT_EQ(1, count_of(svg, "<clipPath"));
}

TEST(SvmToSvg, ellipse) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_ELLIPSE_ACTION)
                                        .rectangle(100, 100, 500, 400)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos,
            svg.find("<ellipse cx=\"300\" cy=\"250\" rx=\"200\" ry=\"150\""));
}

TEST(SvmToSvg, round_rectangle) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_ROUNDRECT_ACTION)
                                        .rectangle(600, 100, 1000, 400)
                                        .u32(60)
                                        .u32(40)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos,
            svg.find("<rect x=\"600\" y=\"100\" width=\"400\" height=\"300\""
                     " rx=\"60\" ry=\"40\""));
}

/// The same geometry LibreOffice was asked to draw: it puts the arc's ends at
/// (441,544) and (100,650).
TEST(SvmToSvg, arc) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_ARC_ACTION)
                                        .rectangle(100, 500, 500, 800)
                                        .point(500, 500)
                                        .point(100, 650)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("d=\"M 441,544 A 200,150 0 0 0 223,511"
                                        " A 200,150 0 0 0 100,650\""));
  EXPECT_NE(std::string::npos, svg.find("fill:none"));
}

/// A pie closes through the centre of its ellipse, a chord straight back.
TEST(SvmToSvg, pie_and_chord_are_closed) {
  const std::string pie = translate(SvmBuilder()
                                        .action(svm::META_PIE_ACTION)
                                        .rectangle(100, 500, 500, 800)
                                        .point(500, 500)
                                        .point(100, 650)
                                        .end()
                                        .file());
  const std::string chord = translate(SvmBuilder()
                                          .action(svm::META_CHORD_ACTION)
                                          .rectangle(100, 500, 500, 800)
                                          .point(500, 500)
                                          .point(100, 650)
                                          .end()
                                          .file());

  EXPECT_NE(std::string::npos, pie.find("d=\"M 300,650 L 441,544 A"));
  EXPECT_NE(std::string::npos, pie.find("100,650 Z\""));
  EXPECT_NE(std::string::npos, chord.find("d=\"M 441,544 A"));
  EXPECT_NE(std::string::npos, chord.find("100,650 Z\""));
}

TEST(SvmToSvg, line) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_LINE_ACTION)
                                        .point(1, 2)
                                        .point(3, 4)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("d=\"M 1,2 L 3,4\""));
}

/// A dot is a zero-length path with a round cap, which a browser draws and a
/// zero-length `<line>` does not. A `PIXEL` brings its own colour and leaves
/// the state's alone.
TEST(SvmToSvg, a_point_and_a_pixel_are_dots) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_LINECOLOR_ACTION)
                                        .u32(0x0000ff)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_POINT_ACTION)
                                        .point(1, 2)
                                        .end()
                                        .action(svm::META_PIXEL_ACTION)
                                        .point(3, 4)
                                        .u32(0x00ff00)
                                        .end()
                                        .action(svm::META_POINT_ACTION)
                                        .point(5, 6)
                                        .end()
                                        .file());

  EXPECT_EQ(3, count_of(svg, "stroke-linecap:round"));
  EXPECT_NE(std::string::npos,
            svg.find("d=\"M 1,2 Z\" style=\"stroke-linecap:round;"
                     "stroke:rgb(0,0,255)"));
  EXPECT_NE(std::string::npos,
            svg.find("d=\"M 3,4 Z\" style=\"stroke-linecap:round;"
                     "stroke:rgb(0,255,0)"));
  EXPECT_NE(std::string::npos,
            svg.find("d=\"M 5,6 Z\" style=\"stroke-linecap:round;"
                     "stroke:rgb(0,0,255)"));
}

/// The ramp runs from the top of the bounds to the bottom, turned about the
/// centre - the same vector LibreOffice's own export writes.
TEST(SvmToSvg, linear_gradient) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_GRADIENT_ACTION)
                    .rectangle(100, 100, 400, 400)
                    .gradient(svm::GRADIENT_LINEAR, 0xff0000, 0x0000ff)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos,
            svg.find("<linearGradient id=\"odr-gradient-1\""
                     " gradientUnits=\"userSpaceOnUse\""
                     " x1=\"250\" y1=\"100\" x2=\"250\" y2=\"400\""));
  EXPECT_NE(std::string::npos,
            svg.find("<stop offset=\"0\" stop-color=\"rgb(255,0,0)\""));
  EXPECT_NE(std::string::npos,
            svg.find("<stop offset=\"1\" stop-color=\"rgb(0,0,255)\""));
  EXPECT_NE(std::string::npos, svg.find("fill:url(#odr-gradient-1)"));
}

/// At 45 degrees the ramp grows to cover the corners it now runs into:
/// LibreOffice puts it from (450,100) to (750,400), and so do we.
TEST(SvmToSvg, a_turned_gradient_covers_the_corners) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_GRADIENT_ACTION)
                    .rectangle(450, 100, 750, 400)
                    .gradient(svm::GRADIENT_LINEAR, 0xff0000, 0x0000ff, 450)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos,
            svg.find("x1=\"450\" y1=\"100\" x2=\"750\" y2=\"400\""));
}

/// `DrawLinearGradient` swaps the colours of an axial ramp: the end colour is
/// at both ends of the axis and the start colour in the middle.
TEST(SvmToSvg, an_axial_gradient_has_its_colours_the_other_way_round) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_GRADIENT_ACTION)
                    .rectangle(100, 100, 400, 400)
                    .gradient(svm::GRADIENT_AXIAL, 0xff0000, 0x0000ff)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos,
            svg.find("<stop offset=\"0\" stop-color=\"rgb(0,0,255)\""));
  EXPECT_NE(std::string::npos,
            svg.find("<stop offset=\"0.5\" stop-color=\"rgb(255,0,0)\""));
}

/// The other complex styles are an ellipse: `r` is the circle the transform
/// stretches sideways, so it is the *smaller* radius, never the larger.
TEST(SvmToSvg, an_elliptical_gradient_is_a_stretched_circle) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_GRADIENT_ACTION)
                    .rectangle(0, 0, 400, 200)
                    .gradient(svm::GRADIENT_ELLIPTICAL, 0xff0000, 0x0000ff)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos, svg.find("cx=\"200\" cy=\"100\" r=\"141.421\""));
  EXPECT_NE(std::string::npos,
            svg.find("gradientTransform=\"matrix(2 0 0 1 -200 0)\""));
}

/// vcl draws a radial ramp as rings shrinking inwards from the start colour,
/// so the end colour is the one in the middle.
TEST(SvmToSvg, a_radial_gradient_ends_in_the_middle) {
  const std::string svg =
      translate(SvmBuilder()
                    .action(svm::META_GRADIENT_ACTION)
                    .rectangle(0, 0, 300, 400)
                    .gradient(svm::GRADIENT_RADIAL, 0xff0000, 0x0000ff)
                    .end()
                    .file());

  EXPECT_NE(std::string::npos, svg.find("<radialGradient"));
  EXPECT_NE(std::string::npos, svg.find("cx=\"150\" cy=\"200\" r=\"250\""));
  EXPECT_NE(std::string::npos,
            svg.find("<stop offset=\"0\" stop-color=\"rgb(0,0,255)\""));
}

TEST(SvmToSvg, hatch) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_HATCH_ACTION)
                                        .poly_rectangle(0, 0, 300, 300)
                                        .begin()
                                        .u16(svm::HATCH_DOUBLE)
                                        .object_color(0x008000)
                                        .i32(100)
                                        .u16(450)
                                        .end()
                                        .end()
                                        .file());

  EXPECT_NE(
      std::string::npos,
      svg.find("<pattern id=\"odr-hatch-1\" patternUnits=\"userSpaceOnUse\""
               " width=\"100\" height=\"100\""
               " patternTransform=\"rotate(-45)\""));
  EXPECT_EQ(2, count_of(svg, "stroke:rgb(0,128,0)"));
  EXPECT_NE(std::string::npos, svg.find("fill:url(#odr-hatch-1)"));
}

/// The state's own colours at a transparency, and the shape as a whole.
TEST(SvmToSvg, transparent) {
  const std::string svg = translate(SvmBuilder()
                                        .action(svm::META_FILLCOLOR_ACTION)
                                        .u32(0x0000ff)
                                        .u8(1)
                                        .end()
                                        .action(svm::META_TRANSPARENT_ACTION)
                                        .poly_rectangle(0, 0, 100, 100)
                                        .u16(60)
                                        .end()
                                        .file());

  EXPECT_NE(std::string::npos, svg.find("fill:rgb(0,0,255)"));
  EXPECT_NE(std::string::npos, svg.find("opacity:0.4"));
}
