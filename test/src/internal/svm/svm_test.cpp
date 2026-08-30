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
                                        .action(svm::META_FONT_ACTION)
                                        .begin()
                                        .ascii_string("a\"b")
                                        .ascii_string("")
                                        .point(0, 10)
                                        .u16(11) // charset
                                        .u16(0)  // family
                                        .u16(0)  // pitch
                                        .u16(0)  // weight
                                        .u16(0)  // underline
                                        .u16(0)  // strikeout
                                        .u16(0)  // italic
                                        .u16(0)  // language
                                        .u16(0)  // width
                                        .u16(0)  // orientation
                                        .u8(0)   // wordline
                                        .u8(0)   // outline
                                        .u8(0)   // shadow
                                        .u8(0)   // kerning
                                        .end()
                                        .end()
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
