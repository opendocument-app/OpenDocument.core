#pragma once

#include <odr/exceptions.hpp>

#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// https://github.com/LibreOffice/core/blob/master/include/vcl/metaact.hxx
namespace odr::internal::svm {

/// `rtl_TextEncoding`, what a byte string in the file is written in. Only the
/// ones we have a decoder for are named; `read_string_with_encoding` takes
/// anything else for `MS_1252`.
enum TextEncoding {
  RTL_TEXTENCODING_DONTKNOW = 0,
  RTL_TEXTENCODING_MS_1252 = 1,
  RTL_TEXTENCODING_APPLE_ROMAN = 2,
  RTL_TEXTENCODING_ASCII_US = 11,
  RTL_TEXTENCODING_ISO_8859_1 = 12,
  RTL_TEXTENCODING_ISO_8859_2 = 13,
  RTL_TEXTENCODING_ISO_8859_3 = 14,
  RTL_TEXTENCODING_ISO_8859_4 = 15,
  RTL_TEXTENCODING_ISO_8859_5 = 16,
  RTL_TEXTENCODING_ISO_8859_6 = 17,
  RTL_TEXTENCODING_ISO_8859_7 = 18,
  RTL_TEXTENCODING_ISO_8859_8 = 19,
  RTL_TEXTENCODING_ISO_8859_14 = 21,
  RTL_TEXTENCODING_ISO_8859_15 = 22,
  RTL_TEXTENCODING_IBM_866 = 30,
  RTL_TEXTENCODING_MS_874 = 32,
  RTL_TEXTENCODING_MS_1250 = 33,
  RTL_TEXTENCODING_MS_1251 = 34,
  RTL_TEXTENCODING_MS_1253 = 35,
  RTL_TEXTENCODING_MS_1254 = 36,
  RTL_TEXTENCODING_MS_1255 = 37,
  RTL_TEXTENCODING_MS_1256 = 38,
  RTL_TEXTENCODING_MS_1257 = 39,
  RTL_TEXTENCODING_MS_1258 = 40,
  RTL_TEXTENCODING_APPLE_CYRILLIC = 44,
  RTL_TEXTENCODING_KOI8_R = 74,
  RTL_TEXTENCODING_UTF8 = 76,
  RTL_TEXTENCODING_ISO_8859_10 = 77,
  RTL_TEXTENCODING_ISO_8859_13 = 78,
  RTL_TEXTENCODING_KOI8_U = 88,
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

/// `PolyFlags`: what a polygon's point is. Two `CONTROL` points between two
/// corners are a bezier segment; everything else is a line.
enum MetaPolyFlags {
  POLY_NORMAL = 0,
  POLY_SMOOTH = 1,
  POLY_CONTROL = 2,
  POLY_SYMMETRIC = 3,
};

/// `MapUnit`, the unit a map mode's coordinates are in. `MAP_RELATIVE` has
/// none of its own: it composes with the map mode before it.
enum MetaMapUnit {
  MAP_100TH_MM = 0,
  MAP_10TH_MM = 1,
  MAP_MM = 2,
  MAP_CM = 3,
  MAP_1000TH_INCH = 4,
  MAP_100TH_INCH = 5,
  MAP_10TH_INCH = 6,
  MAP_INCH = 7,
  MAP_POINT = 8,
  MAP_TWIP = 9,
  MAP_PIXEL = 10,
  MAP_SYS_FONT = 11,
  MAP_APP_FONT = 12,
  MAP_RELATIVE = 13,
};

/// `awt::GradientStyle`.
enum MetaGradientStyle {
  GRADIENT_LINEAR = 0,
  GRADIENT_AXIAL = 1,
  GRADIENT_RADIAL = 2,
  GRADIENT_ELLIPTICAL = 3,
  GRADIENT_SQUARE = 4,
  GRADIENT_RECT = 5,
};

/// `HatchStyle`: one set of lines, two crossing, or three.
enum MetaHatchStyle {
  HATCH_SINGLE = 0,
  HATCH_DOUBLE = 1,
  HATCH_TRIPLE = 2,
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

struct PixelAction final {
  IntPair point;
  std::uint32_t color{};
};

struct LineAction final {
  IntPair start;
  IntPair end;
  LineInfo line_info;
};

struct RoundRectangleAction final {
  Rectangle rectangle;
  /// The corner ellipse's radii.
  std::uint32_t horizontal_round{};
  std::uint32_t vertical_round{};
};

/// `ARC`, `PIE` and `CHORD`: the ellipse, and the two rays that cut it.
struct ArcAction final {
  Rectangle rectangle;
  IntPair start;
  IntPair end;
};

/// A polygon, and - where the file carried them - what its points are.
struct Polygon final {
  std::vector<IntPair> points;
  /// One `PolyFlags` per point, or empty where every point is a corner.
  std::vector<std::uint8_t> flags;
};

struct PolyLineAction final {
  Polygon polygon;
  LineInfo line_info;
};

struct PolygonAction final {
  Polygon polygon;
};

struct PolyPolygonAction final {
  std::vector<Polygon> polygons;
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

/// `Gradient`: the two colours, which way the ramp runs, and where it sits.
struct Gradient final {
  std::uint16_t style{};
  std::uint32_t start_color{};
  std::uint32_t end_color{};
  /// Tenths of a degree, counter-clockwise.
  std::uint16_t angle{};
  /// Percent of the ramp the start colour holds before it ramps.
  std::uint16_t border{};
  /// Percent of the bounds, where a complex gradient's centre sits.
  std::uint16_t offset_x{};
  std::uint16_t offset_y{};
  /// Percent, what the colours are scaled by.
  std::uint16_t start_intensity{100};
  std::uint16_t end_intensity{100};
  std::uint16_t step_count{};
};

/// `Hatch`: the lines drawn across a shape.
struct Hatch final {
  std::uint16_t style{};
  std::uint32_t color{};
  std::int32_t distance{};
  /// Tenths of a degree, counter-clockwise.
  std::uint16_t angle{};
};

/// A clip region: bands covering it as a union of rectangles, and from
/// version 2 the poly-polygon those were rasterised from. An *empty* region
/// covers nothing and so clips everything away; `REGION_NULL`, which does not
/// clip at all, is no region and reads as `std::nullopt`.
struct Region final {
  std::vector<Rectangle> rectangles;
  std::vector<Polygon> polygons;
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
Polygon read_polygon(std::istream &in);
/// `Polygon::Read`: the points again, and the flags behind them - what a
/// polygon carrying curves is written as.
Polygon read_flagged_polygon(std::istream &in);
std::vector<Polygon> read_poly_polygon(std::istream &in);

Header read_header(std::istream &in);
ActionHeader read_action_header(std::istream &in);
MapMode read_map_mode(std::istream &in);
LineInfo read_line_info(std::istream &in);
Font read_font(std::istream &in);
PixelAction read_pixel_action(std::istream &in);
LineAction read_line_action(std::istream &in, const VersionLength &vl);
RoundRectangleAction read_round_rectangle_action(std::istream &in);
ArcAction read_arc_action(std::istream &in);
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
/// A colour *inside* an object, which is not the plain `uint32` an action's
/// own colour is: `GenericTypeSerializer::readColor` reads a name id, and only
/// the user one carries three 16-bit channels behind it.
std::uint32_t read_object_color(std::istream &in);
Gradient read_gradient(std::istream &in);
Hatch read_hatch(std::istream &in);
/// A region, as `ReadRegion` reads one: a band list, and from version 2 the
/// poly-polygon it came from. `std::nullopt` where it does not clip at all.
std::optional<Region> read_region(std::istream &in);
/// A `CLIPREGION`: the region, and whether it clips at all.
std::pair<std::optional<Region>, bool>
read_clip_region_action(std::istream &in);
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
