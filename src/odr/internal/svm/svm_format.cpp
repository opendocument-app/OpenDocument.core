#include <odr/internal/svm/svm_format.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace odr::internal {

namespace {

std::string read_bytes(std::istream &in, const std::uint64_t size) {
  try {
    return util::byte_stream::read_u8s(in, size);
  } catch (const std::runtime_error &) {
    throw MalformedSvmFile();
  }
}

} // namespace

std::string svm::read_ascii_string(std::istream &in,
                                   const std::uint32_t length) {
  return read_bytes(in, length);
}

std::string svm::read_utf16_string(std::istream &in,
                                   const std::uint32_t length) {
  const std::string bytes =
      read_bytes(in, static_cast<std::uint64_t>(length) * 2);
  std::u16string result_u16(length, u' ');
  std::memcpy(result_u16.data(), bytes.data(), bytes.size());
  return util::string::u16string_to_string(result_u16);
}

std::string svm::read_uint16_prefixed_ascii_string(std::istream &in) {
  std::uint16_t length;
  read_primitive(in, length);
  return read_ascii_string(in, length);
}

std::string svm::read_uint32_prefixed_utf16_string(std::istream &in) {
  std::uint32_t length;
  read_primitive(in, length);
  return read_utf16_string(in, length);
}

std::string svm::read_uint16_prefixed_utf16_string(std::istream &in) {
  std::uint16_t length;
  read_primitive(in, length);
  return read_utf16_string(in, length);
}

std::string svm::read_string_with_encoding(std::istream &in,
                                           const TextEncoding encoding) {
  if (encoding == RTL_TEXTENCODING_UCS2) {
    return read_uint32_prefixed_utf16_string(in);
  }
  return read_uint16_prefixed_ascii_string(in);
}

std::string_view svm::action_type_name(const std::uint16_t type) {
  switch (type) {
  case META_NULL_ACTION:
    return "META_NULL_ACTION";
  case META_PIXEL_ACTION:
    return "META_PIXEL_ACTION";
  case META_POINT_ACTION:
    return "META_POINT_ACTION";
  case META_LINE_ACTION:
    return "META_LINE_ACTION";
  case META_RECT_ACTION:
    return "META_RECT_ACTION";
  case META_ROUNDRECT_ACTION:
    return "META_ROUNDRECT_ACTION";
  case META_ELLIPSE_ACTION:
    return "META_ELLIPSE_ACTION";
  case META_ARC_ACTION:
    return "META_ARC_ACTION";
  case META_PIE_ACTION:
    return "META_PIE_ACTION";
  case META_CHORD_ACTION:
    return "META_CHORD_ACTION";
  case META_POLYLINE_ACTION:
    return "META_POLYLINE_ACTION";
  case META_POLYGON_ACTION:
    return "META_POLYGON_ACTION";
  case META_POLYPOLYGON_ACTION:
    return "META_POLYPOLYGON_ACTION";
  case META_TEXT_ACTION:
    return "META_TEXT_ACTION";
  case META_TEXTARRAY_ACTION:
    return "META_TEXTARRAY_ACTION";
  case META_STRETCHTEXT_ACTION:
    return "META_STRETCHTEXT_ACTION";
  case META_TEXTRECT_ACTION:
    return "META_TEXTRECT_ACTION";
  case META_BMP_ACTION:
    return "META_BMP_ACTION";
  case META_BMPSCALE_ACTION:
    return "META_BMPSCALE_ACTION";
  case META_BMPSCALEPART_ACTION:
    return "META_BMPSCALEPART_ACTION";
  case META_BMPEX_ACTION:
    return "META_BMPEX_ACTION";
  case META_BMPEXSCALE_ACTION:
    return "META_BMPEXSCALE_ACTION";
  case META_BMPEXSCALEPART_ACTION:
    return "META_BMPEXSCALEPART_ACTION";
  case META_MASK_ACTION:
    return "META_MASK_ACTION";
  case META_MASKSCALE_ACTION:
    return "META_MASKSCALE_ACTION";
  case META_MASKSCALEPART_ACTION:
    return "META_MASKSCALEPART_ACTION";
  case META_GRADIENT_ACTION:
    return "META_GRADIENT_ACTION";
  case META_HATCH_ACTION:
    return "META_HATCH_ACTION";
  case META_WALLPAPER_ACTION:
    return "META_WALLPAPER_ACTION";
  case META_CLIPREGION_ACTION:
    return "META_CLIPREGION_ACTION";
  case META_ISECTRECTCLIPREGION_ACTION:
    return "META_ISECTRECTCLIPREGION_ACTION";
  case META_ISECTREGIONCLIPREGION_ACTION:
    return "META_ISECTREGIONCLIPREGION_ACTION";
  case META_MOVECLIPREGION_ACTION:
    return "META_MOVECLIPREGION_ACTION";
  case META_LINECOLOR_ACTION:
    return "META_LINECOLOR_ACTION";
  case META_FILLCOLOR_ACTION:
    return "META_FILLCOLOR_ACTION";
  case META_TEXTCOLOR_ACTION:
    return "META_TEXTCOLOR_ACTION";
  case META_TEXTFILLCOLOR_ACTION:
    return "META_TEXTFILLCOLOR_ACTION";
  case META_TEXTALIGN_ACTION:
    return "META_TEXTALIGN_ACTION";
  case META_MAPMODE_ACTION:
    return "META_MAPMODE_ACTION";
  case META_FONT_ACTION:
    return "META_FONT_ACTION";
  case META_PUSH_ACTION:
    return "META_PUSH_ACTION";
  case META_POP_ACTION:
    return "META_POP_ACTION";
  case META_RASTEROP_ACTION:
    return "META_RASTEROP_ACTION";
  case META_TRANSPARENT_ACTION:
    return "META_TRANSPARENT_ACTION";
  case META_EPS_ACTION:
    return "META_EPS_ACTION";
  case META_REFPOINT_ACTION:
    return "META_REFPOINT_ACTION";
  case META_TEXTLINECOLOR_ACTION:
    return "META_TEXTLINECOLOR_ACTION";
  case META_TEXTLINE_ACTION:
    return "META_TEXTLINE_ACTION";
  case META_FLOATTRANSPARENT_ACTION:
    return "META_FLOATTRANSPARENT_ACTION";
  case META_GRADIENTEX_ACTION:
    return "META_GRADIENTEX_ACTION";
  case META_LAYOUTMODE_ACTION:
    return "META_LAYOUTMODE_ACTION";
  case META_TEXTLANGUAGE_ACTION:
    return "META_TEXTLANGUAGE_ACTION";
  case META_OVERLINECOLOR_ACTION:
    return "META_OVERLINECOLOR_ACTION";
  case META_COMMENT_ACTION:
    return "META_COMMENT_ACTION";
  default:
    return "UNKNOWN";
  }
}

svm::VersionLength svm::read_version_length(std::istream &in) {
  VersionLength result;
  read_primitive(in, result.version);
  read_primitive(in, result.length);
  if (result.version <= 0) {
    // TODO log or throw illegal version
  }
  return result;
}

svm::IntPair svm::read_int_pair(std::istream &in) {
  IntPair result;
  read_primitive(in, result.x);
  read_primitive(in, result.y);
  return result;
}

svm::Rectangle svm::read_rectangle(std::istream &in) {
  Rectangle result;
  read_primitive(in, result.left);
  read_primitive(in, result.top);
  read_primitive(in, result.right);
  read_primitive(in, result.bottom);
  return result;
}

std::vector<svm::IntPair> svm::read_polygon(std::istream &in) {
  std::vector<IntPair> result;

  std::uint16_t size;
  read_primitive(in, size);

  result.resize(size);
  for (auto &&p : result) {
    p = read_int_pair(in);
  }

  return result;
}

std::vector<std::vector<svm::IntPair>>
svm::read_poly_polygon(std::istream &in) {
  std::vector<std::vector<IntPair>> result;

  std::uint16_t size;
  read_primitive(in, size);

  result.resize(size);
  for (auto &&p : result) {
    p = read_polygon(in);
  }

  return result;
}

svm::Header svm::read_header(std::istream &in) {
  Header result;

  std::array<char, 6> magic{};
  in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
  if (!in || std::memcmp("VCLMTF", magic.data(), magic.size()) != 0) {
    throw NoSvmFile();
  }

  result.vl = read_version_length(in);

  const std::int64_t start = in.tellg();
  read_primitive(in, result.compression_mode);
  result.map_mode = read_map_mode(in);
  result.size = read_int_pair(in);
  read_primitive(in, result.action_count);

  if (result.vl.version >= 2) {
    read_primitive(in, result.render_graphic_replacements);
  }

  // Only skip forward: reading past the declared length would otherwise wrap
  // the difference and swallow the rest of the stream.
  if (const std::int64_t left =
          result.vl.length - (static_cast<std::int64_t>(in.tellg()) - start);
      left > 0) {
    // TODO log header skipping bytes
    in.ignore(static_cast<std::streamsize>(left));
  }

  return result;
}

svm::ActionHeader svm::read_action_header(std::istream &in) {
  ActionHeader result;

  read_primitive(in, result.type);
  result.vl = read_version_length(in);

  return result;
}

svm::MapMode svm::read_map_mode(std::istream &in) {
  MapMode result;

  read_version_length(in);

  read_primitive(in, result.unit);
  result.origin = read_int_pair(in);
  result.scale_x = read_int_pair(in);
  result.scale_y = read_int_pair(in);
  read_primitive(in, result.simple);

  // the scales are fractions, and every coordinate goes through them
  if (result.scale_x.y == 0 || result.scale_y.y == 0) {
    throw MalformedSvmFile();
  }

  return result;
}

svm::LineInfo svm::read_line_info(std::istream &in) {
  LineInfo result;

  auto [version, length] = read_version_length(in);

  read_primitive(in, result.line_style);
  read_primitive(in, result.width);

  if (version >= 2) {
    read_primitive(in, result.dash_count);
    read_primitive(in, result.dash_length);
    read_primitive(in, result.dot_count);
    read_primitive(in, result.dot_length);
    read_primitive(in, result.distance);
  }

  if (version >= 3) {
    read_primitive(in, result.line_join);
  }

  if (version >= 4) {
    // TODO log version 4 not implemented
  }

  return result;
}

svm::Font svm::read_font(std::istream &in) {
  Font result;

  result.vl = read_version_length(in);
  result.family_name = read_uint16_prefixed_ascii_string(in);
  result.style_name = read_uint16_prefixed_ascii_string(in);
  result.size = read_int_pair(in);
  read_primitive(in, result.charset);
  read_primitive(in, result.family);
  read_primitive(in, result.pitch);
  read_primitive(in, result.weight);
  read_primitive(in, result.underline);
  read_primitive(in, result.strikeout);
  read_primitive(in, result.italic);
  read_primitive(in, result.language);
  read_primitive(in, result.width);
  read_primitive(in, result.orientation);
  read_primitive(in, result.wordline);
  read_primitive(in, result.outline);
  read_primitive(in, result.shadow);
  read_primitive(in, result.kerning);

  if (result.vl.version >= 2) {
    read_primitive(in, result.relief);
    read_primitive(in, result.cjk_language);
    read_primitive(in, result.vertical);
    read_primitive(in, result.emphasis_mark);
  }

  if (result.vl.version >= 3) {
    read_primitive(in, result.overline);
  }

  return result;
}

svm::PolyLineAction svm::read_poly_line_action(std::istream &in,
                                               const VersionLength &vl) {
  PolyLineAction result;

  result.points = read_polygon(in);

  if (vl.version >= 2) {
    result.line_info = read_line_info(in);
  }

  if (vl.version >= 3) {
    bool has_flags;
    read_primitive(in, has_flags);

    if (has_flags) {
      // TODO flags not implemented
    }
  }

  return result;
}

svm::PolygonAction svm::read_polygon_action(std::istream &in,
                                            const VersionLength &vl) {
  PolygonAction result;

  result.points = read_polygon(in);

  if (vl.version >= 3) {
    bool has_flags;
    read_primitive(in, has_flags);

    if (has_flags) {
      // TODO flags not implemented
    }
  }

  return result;
}

svm::PolyPolygonAction svm::read_poly_polygon_action(std::istream &in,
                                                     const VersionLength &vl) {
  PolyPolygonAction result;

  result.polygons = read_poly_polygon(in);

  if (vl.version >= 2) {
    std::uint16_t complex_polygons;
    read_primitive(in, complex_polygons);

    if (complex_polygons > 0) {
      // TODO complex not implemented
    }
  }

  return result;
}

svm::TextAction svm::read_text_action(std::istream &in, const VersionLength &vl,
                                      const TextEncoding encoding) {
  TextAction result;

  result.point = read_int_pair(in);
  result.text = read_string_with_encoding(in, encoding);
  read_primitive(in, result.offset);
  read_primitive(in, result.length);

  if (vl.version >= 2) {
    result.text = read_uint16_prefixed_utf16_string(in);
  }

  return result;
}

svm::TextArrayAction svm::read_text_array_action(std::istream &in,
                                                 const VersionLength &vl,
                                                 const TextEncoding encoding) {
  TextArrayAction result;

  result.point = read_int_pair(in);
  result.text = read_string_with_encoding(in, encoding);
  read_primitive(in, result.offset);
  read_primitive(in, result.length);
  std::uint32_t dx_array_length;
  read_primitive(in, dx_array_length);
  // grown entry by entry: the declared length is only trustworthy as far as the
  // stream actually reaches
  for (std::uint32_t i = 0; i < dx_array_length; ++i) {
    std::uint32_t dx;
    read_primitive(in, dx);
    result.dx_array.push_back(dx);
  }

  if (vl.version >= 2) {
    result.text = read_uint16_prefixed_utf16_string(in);
  }

  return result;
}

svm::StretchTextAction
svm::read_stretch_text_action(std::istream &in, const VersionLength &vl,
                              const TextEncoding encoding) {
  StretchTextAction result;

  result.point = read_int_pair(in);
  result.text = read_string_with_encoding(in, encoding);
  read_primitive(in, result.width);
  read_primitive(in, result.offset);
  read_primitive(in, result.length);

  if (vl.version >= 2) {
    result.text = read_uint16_prefixed_utf16_string(in);
  }

  return result;
}

svm::TextRectangleAction
svm::read_text_rectangle_action(std::istream &in, const VersionLength &vl,
                                const TextEncoding encoding) {
  TextRectangleAction result;

  result.rectangle = read_rectangle(in);
  result.text = read_string_with_encoding(in, encoding);
  read_primitive(in, result.style);

  if (vl.version >= 2) {
    result.text = read_uint16_prefixed_utf16_string(in);
  }

  return result;
}

std::uint16_t svm::read_push_action(std::istream &in, const VersionLength &vl) {
  if (vl.length < sizeof(std::uint16_t)) {
    return PUSH_ALL;
  }

  std::uint16_t result;
  read_primitive(in, result);
  return result;
}

svm::TextLineAction svm::read_text_line_action(std::istream &in,
                                               const VersionLength &vl) {
  TextLineAction result;

  result.position = read_int_pair(in);
  read_primitive(in, result.width);
  read_primitive(in, result.strikeout);
  read_primitive(in, result.underline);

  if (vl.version >= 2) {
    read_primitive(in, result.overline);
  }

  return result;
}

} // namespace odr::internal
