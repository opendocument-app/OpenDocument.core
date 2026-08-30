#pragma once

#include <odr/exceptions.hpp>

#include <cstdint>
#include <istream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// https://github.com/LibreOffice/core/blob/master/include/vcl/metaact.hxx
namespace odr::internal::svm {

enum TextEncoding {
  RTL_TEXTENCODING_DONTKNOW = 0,
  RTL_TEXTENCODING_ASCII_US = 11,
  RTL_TEXTENCODING_UCS4 = 0xFFFE,
  RTL_TEXTENCODING_UCS2 = 0xFFFF,
};

/// `TextAlign`: which edge of the text the draw point names. Vertical only -
/// vcl has no horizontal text alignment, a run always starts at the point.
enum MetaTextAlign {
  ALIGN_TOP = 0,
  ALIGN_BASELINE = 1,
  ALIGN_BOTTOM = 2,
};

/// `FontWeight`.
enum MetaFontWeight {
  WEIGHT_DONTKNOW = 0,
  WEIGHT_THIN = 1,
  WEIGHT_ULTRALIGHT = 2,
  WEIGHT_LIGHT = 3,
  WEIGHT_SEMILIGHT = 4,
  WEIGHT_NORMAL = 5,
  WEIGHT_MEDIUM = 6,
  WEIGHT_SEMIBOLD = 7,
  WEIGHT_BOLD = 8,
  WEIGHT_ULTRABOLD = 9,
  WEIGHT_BLACK = 10,
};

/// `FontItalic`.
enum MetaFontItalic {
  ITALIC_NONE = 0,
  ITALIC_OBLIQUE = 1,
  ITALIC_NORMAL = 2,
  ITALIC_DONTKNOW = 3,
};

/// `FontLineStyle`, the underline; anything but `NONE` underlines.
enum MetaFontLineStyle {
  LINESTYLE_NONE = 0,
};

/// `FontStrikeout`; anything but `NONE` strikes through.
enum MetaFontStrikeout {
  STRIKEOUT_NONE = 0,
};

/// `LineStyle`, what a `LineInfo` draws with.
enum MetaLineStyle {
  LINE_NONE = 0,
  LINE_SOLID = 1,
  LINE_DASH = 2,
};

/// `vcl::PushFlags`: what a `PUSH` saves; anything not named survives the
/// `POP`.
enum PushFlags : std::uint16_t {
  PUSH_LINECOLOR = 0x0001,
  PUSH_FILLCOLOR = 0x0002,
  PUSH_FONT = 0x0004,
  PUSH_TEXTCOLOR = 0x0008,
  PUSH_MAPMODE = 0x0010,
  PUSH_CLIPREGION = 0x0020,
  PUSH_RASTEROP = 0x0040,
  PUSH_TEXTFILLCOLOR = 0x0080,
  PUSH_TEXTALIGN = 0x0100,
  PUSH_REFPOINT = 0x0200,
  PUSH_TEXTLINECOLOR = 0x0400,
  PUSH_TEXTLAYOUTMODE = 0x0800,
  PUSH_TEXTLANGUAGE = 0x1000,
  PUSH_OVERLINECOLOR = 0x2000,
  PUSH_ALL = 0xffff,
};

enum MetaActionType {
  META_NULL_ACTION = 0,
  META_PIXEL_ACTION = 100,
  META_POINT_ACTION = 101,
  META_LINE_ACTION = 102,
  META_RECT_ACTION = 103,
  META_ROUNDRECT_ACTION = 104,
  META_ELLIPSE_ACTION = 105,
  META_ARC_ACTION = 106,
  META_PIE_ACTION = 107,
  META_CHORD_ACTION = 108,
  META_POLYLINE_ACTION = 109,
  META_POLYGON_ACTION = 110,
  META_POLYPOLYGON_ACTION = 111,
  META_TEXT_ACTION = 112,
  META_TEXTARRAY_ACTION = 113,
  META_STRETCHTEXT_ACTION = 114,
  META_TEXTRECT_ACTION = 115,
  META_BMP_ACTION = 116,
  META_BMPSCALE_ACTION = 117,
  META_BMPSCALEPART_ACTION = 118,
  META_BMPEX_ACTION = 119,
  META_BMPEXSCALE_ACTION = 120,
  META_BMPEXSCALEPART_ACTION = 121,
  META_MASK_ACTION = 122,
  META_MASKSCALE_ACTION = 123,
  META_MASKSCALEPART_ACTION = 124,
  META_GRADIENT_ACTION = 125,
  META_HATCH_ACTION = 126,
  META_WALLPAPER_ACTION = 127,
  META_CLIPREGION_ACTION = 128,
  META_ISECTRECTCLIPREGION_ACTION = 129,
  META_ISECTREGIONCLIPREGION_ACTION = 130,
  META_MOVECLIPREGION_ACTION = 131,
  META_LINECOLOR_ACTION = 132,
  META_FILLCOLOR_ACTION = 133,
  META_TEXTCOLOR_ACTION = 134,
  META_TEXTFILLCOLOR_ACTION = 135,
  META_TEXTALIGN_ACTION = 136,
  META_MAPMODE_ACTION = 137,
  META_FONT_ACTION = 138,
  META_PUSH_ACTION = 139,
  META_POP_ACTION = 140,
  META_RASTEROP_ACTION = 141,
  META_TRANSPARENT_ACTION = 142,
  META_EPS_ACTION = 143,
  META_REFPOINT_ACTION = 144,
  META_TEXTLINECOLOR_ACTION = 145,
  META_TEXTLINE_ACTION = 146,
  META_FLOATTRANSPARENT_ACTION = 147,
  META_GRADIENTEX_ACTION = 148,
  META_LAYOUTMODE_ACTION = 149,
  META_TEXTLANGUAGE_ACTION = 150,
  META_OVERLINECOLOR_ACTION = 151,
  META_COMMENT_ACTION = 512,
};

struct VersionLength final {
  std::uint16_t version{};
  std::uint32_t length{};
};

struct IntPair final {
  std::int32_t x{};
  std::int32_t y{};
};

struct Rectangle final {
  std::int32_t left{};
  std::int32_t top{};
  std::int32_t right{};
  std::int32_t bottom{};
};

struct MapMode final {
  std::uint16_t unit{};
  IntPair origin;
  IntPair scale_x;
  IntPair scale_y;
  bool simple{};
};

struct LineInfo final {
  std::uint16_t line_style{};
  std::int32_t width{};

  std::uint16_t dash_count{};
  std::int32_t dash_length{};
  std::uint16_t dot_count{};
  std::int32_t dot_length{};
  std::int32_t distance{};

  std::uint16_t line_join{};
};

struct Font final {
  VersionLength vl;
  std::string family_name;
  std::string style_name;
  IntPair size;
  std::uint16_t charset{};
  std::uint16_t family{};
  std::uint16_t pitch{};
  std::uint16_t weight{};
  std::uint16_t underline{};
  std::uint16_t strikeout{};
  std::uint16_t italic{};
  std::uint16_t language{};
  std::uint16_t width{};
  std::uint16_t orientation{};
  bool wordline{};
  bool outline{};
  bool shadow{};
  std::uint8_t kerning{};

  // version 2
  std::uint8_t relief{};
  std::uint16_t cjk_language{};
  bool vertical{};
  std::uint16_t emphasis_mark{};

  // version 3
  std::uint16_t overline{};
};

struct Header final {
  VersionLength vl;
  std::uint32_t compression_mode{};
  MapMode map_mode;
  IntPair size;
  std::uint32_t action_count{};
  std::uint8_t render_graphic_replacements{};
};

struct ActionHeader final {
  std::uint16_t type{};
  VersionLength vl;
};

struct PolyLineAction final {
  std::vector<IntPair> points;
  LineInfo line_info;
};

struct PolygonAction final {
  std::vector<IntPair> points;
};

struct PolyPolygonAction final {
  std::vector<std::vector<IntPair>> polygons;
};

struct TextAction final {
  IntPair point;
  std::string text;
  std::uint16_t offset{};
  std::uint16_t length{};
};

struct TextArrayAction final {
  IntPair point;
  std::string text;
  std::uint16_t offset{};
  std::uint16_t length{};
  std::vector<std::uint32_t> dx_array;
};

struct StretchTextAction final {
  IntPair point;
  std::string text;
  std::uint32_t width{};
  std::uint16_t offset{};
  std::uint16_t length{};
};

struct TextRectangleAction final {
  Rectangle rectangle;
  std::string text;
  std::uint16_t style{};
};

/// A clip region: bands covering it as a union of rectangles, and from
/// version 2 the poly-polygon those were rasterised from.
struct Region final {
  /// `REGION_NULL`: no clipping at all, as against a region that covers
  /// nothing and clips everything away.
  bool null{};
  std::vector<Rectangle> rectangles;
  std::vector<std::vector<IntPair>> polygons;
};

/// A dib as something a browser reads. A metafile stores a dib *with* its
/// `BITMAPFILEHEADER`, so its bytes already are a `.bmp` file; they are only
/// unpacked where a png would be smaller, which for a chart is by fifty.
struct Image final {
  /// Empty where the dib is one we could not hand on, its bytes read anyway.
  std::string data;
  std::string mime_type;
  IntPair size_pixel;
};

struct Bitmap final {
  Image image;
  /// The transparency mask, whose *white* is where @ref image does not show.
  /// Empty where the action carried none.
  Image mask;
};

/// The `BMP` family. @ref size is the logical size to draw at, empty for the
/// actions that draw at the bitmap's own; @ref source_point and
/// @ref source_size name the part to draw, in pixels, and are empty for the
/// actions that draw all of it.
struct BitmapAction final {
  Bitmap bitmap;
  IntPair point;
  IntPair size;
  IntPair source_point;
  IntPair source_size;
};

struct TextLineAction final {
  IntPair position;
  std::int32_t width{};
  std::uint32_t strikeout{};
  std::uint32_t underline{};
  std::uint32_t overline{};
};

/// The action type's name as `metaact.hxx` spells it, or `"UNKNOWN"`.
[[nodiscard]] std::string_view action_type_name(std::uint16_t type);

/// Reads a fixed-size field. A short read leaves the destination untouched, so
/// the stream ending mid-field is malformed input rather than a stale value.
template <typename T> void read_primitive(std::istream &in, T &out) {
  if (!in.read(reinterpret_cast<char *>(&out), sizeof(out))) {
    throw MalformedSvmFile();
  }
}

std::string read_ascii_string(std::istream &in, std::uint32_t length);
std::string read_utf16_string(std::istream &in, std::uint32_t length);
std::string read_uint16_prefixed_ascii_string(std::istream &in);
std::string read_uint32_prefixed_utf16_string(std::istream &in);
std::string read_uint16_prefixed_utf16_string(std::istream &in);
std::u16string read_uint16_prefixed_u16string(std::istream &in);
std::string read_string_with_encoding(std::istream &in, TextEncoding encoding);

VersionLength read_version_length(std::istream &in);
IntPair read_int_pair(std::istream &in);
Rectangle read_rectangle(std::istream &in);
std::vector<IntPair> read_polygon(std::istream &in);
std::vector<std::vector<IntPair>> read_poly_polygon(std::istream &in);

Header read_header(std::istream &in);
ActionHeader read_action_header(std::istream &in);
MapMode read_map_mode(std::istream &in);
LineInfo read_line_info(std::istream &in);
Font read_font(std::istream &in);
PolyLineAction read_poly_line_action(std::istream &in, const VersionLength &vl);
PolygonAction read_polygon_action(std::istream &in, const VersionLength &vl);
PolyPolygonAction read_poly_polygon_action(std::istream &in,
                                           const VersionLength &vl);
TextAction read_text_action(std::istream &in, const VersionLength &vl,
                            TextEncoding encoding);
TextArrayAction read_text_array_action(std::istream &in,
                                       const VersionLength &vl,
                                       TextEncoding encoding);
StretchTextAction read_stretch_text_action(std::istream &in,
                                           const VersionLength &vl,
                                           TextEncoding encoding);
TextRectangleAction read_text_rectangle_action(std::istream &in,
                                               const VersionLength &vl,
                                               TextEncoding encoding);
TextLineAction read_text_line_action(std::istream &in, const VersionLength &vl);
/// The `PushFlags` of a `PUSH`. A version that carries none saves everything.
std::uint16_t read_push_action(std::istream &in, const VersionLength &vl);
/// The `TextAlign` of a `TEXTALIGN`.
std::uint16_t read_text_align_action(std::istream &in);
/// A region, as `ReadRegion` reads one: a band list, and from version 2 the
/// poly-polygon it came from.
Region read_region(std::istream &in);
/// A `CLIPREGION`: the region, and whether it clips at all.
std::pair<Region, bool> read_clip_region_action(std::istream &in);
/// A dib with its file header, as `ReadDIB(…, bFileHeader=true)` reads one.
/// @p limit is what the enclosing action declared, so a length field cannot
/// ask for more than the file holds.
Image read_dib(std::istream &in, std::uint32_t limit);
/// @ref read_dib plus the optional transparency mask behind it, as
/// `ReadDIBBitmapEx` reads one.
Bitmap read_dib_bitmap_ex(std::istream &in, std::uint32_t limit);
/// One of the `BMP` family, @p type saying which.
BitmapAction read_bitmap_action(std::istream &in, std::uint16_t type,
                                const VersionLength &vl);

} // namespace odr::internal::svm
