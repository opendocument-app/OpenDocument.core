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
  SvmBuilder &font(const std::string &family, const std::int32_t size) {
    action(svm::META_FONT_ACTION)
        .begin()
        .ascii_string(family)
        .ascii_string("")
        .point(0, size)
        .u16(11); // charset: ascii
    for (int i = 0; i < 9; ++i) {
      u16(0); // family, pitch, weight, underline, strikeout, italic,
              // language, width, orientation
    }
    for (int i = 0; i < 4; ++i) {
      u8(0); // wordline, outline, shadow, kerning
    }
    return end().end();
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

/// The pen outlines what the brush fills - a shape that sets both draws both.
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

/// A colour the state does not set is not drawn at all.
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

/// A pop restores the state the push saved.
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

/// ...and only that: what the flags do not name outlives the pop. Every
/// second push in the corpus saves this subset, so restoring everything would
/// be wrong far more often than not.
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

/// One shape, one path: a second polygon is a hole in the first, and only the
/// fill rule over a single path cuts it out.
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

/// A polyline carries its own pen, and its width is a length in the drawing.
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

/// The font size is a length in the drawing, so the map mode scales it like
/// every coordinate.
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
