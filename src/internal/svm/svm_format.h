#ifndef ODR_INTERNAL_SVM_FORMAT_H
#define ODR_INTERNAL_SVM_FORMAT_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// https://github.com/LibreOffice/core/blob/master/include/vcl/metaact.hxx
namespace odr::internal::svm {

enum TextEncoding {
  RTL_TEXTENCODING_DONTKNOW = 0,
  RTL_TEXTENCODING_ASCII_US = 11,
  RTL_TEXTENCODING_UCS4 = 0xFFFE,
  RTL_TEXTENCODING_UCS2 = 0xFFFF,
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

struct TextLineAction final {
  IntPair position;
  std::int32_t width{};
  std::uint32_t strikeout{};
  std::uint32_t underline{};
  std::uint32_t overline{};
};

template <typename T> void read_primitive(std::istream &in, T &out) {
  in.read(reinterpret_cast<char *>(&out), sizeof(out));
}

std::string read_ascii_string(std::istream &in, std::uint32_t length);
std::string read_utf16_string(std::istream &in, std::uint32_t length);
std::string read_uint16_prefixed_ascii_string(std::istream &in);
std::string read_uint32_prefixed_utf16_string(std::istream &in);
std::string read_uint16_prefixed_utf16_string(std::istream &in);
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

} // namespace odr::internal::svm

#endif // ODR_INTERNAL_SVM_FORMAT_H
