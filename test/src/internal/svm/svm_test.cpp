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
