#include <cstring>
#include <internal/svm/svm_format.h>
#include <internal/util/string_util.h>
#include <odr/exceptions.h>

namespace odr::internal {

std::string svm::read_ascii_string(std::istream &in,
                                   const std::uint32_t length) {
  std::string result(length, ' ');
  in.read(static_cast<char *>(result.data()), result.size());
  return result;
}

std::string svm::read_utf16_string(std::istream &in,
                                   const std::uint32_t length) {
  std::u16string result_u16(length, ' ');
  in.read(reinterpret_cast<char *>(result_u16.data()), length * 2);
  return util::string::u16string_to_string(result_u16);
}

std::string svm::read_uint16_prefixed_ascii_string(std::istream &in) {
  uint16_t length;
  read_primitive(in, length);
  return read_ascii_string(in, length);
}

std::string svm::read_uint32_prefixed_utf16_string(std::istream &in) {
  uint32_t length;
  read_primitive(in, length);
  return read_utf16_string(in, length);
}

std::string svm::read_uint16_prefixed_utf16_string(std::istream &in) {
  uint16_t length;
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

  char magic[6];
  in.read(magic, sizeof(magic));
  if (std::strncmp("VCLMTF", magic, sizeof(magic)) != 0) {
    throw NoSvmFile();
  }

  result.vl = read_version_length(in);

  std::size_t start = in.tellg();
  read_primitive(in, result.compression_mode);
  result.map_mode = read_map_mode(in);
  result.size = read_int_pair(in);
  read_primitive(in, result.action_count);

  if (result.vl.version >= 2) {
    read_primitive(in, result.render_graphic_replacements);
  }

  const std::size_t left = result.vl.length - ((std::size_t)in.tellg() - start);
  if (left > 0) {
    // TODO log header skipping bytes
    in.ignore(left);
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

  return result;
}

svm::LineInfo svm::read_line_info(std::istream &in) {
  LineInfo result;

  VersionLength vl = read_version_length(in);

  read_primitive(in, result.line_style);
  read_primitive(in, result.width);

  if (vl.version >= 2) {
    read_primitive(in, result.dash_count);
    read_primitive(in, result.dash_length);
    read_primitive(in, result.dot_count);
    read_primitive(in, result.dot_length);
    read_primitive(in, result.distance);
  }

  if (vl.version >= 3) {
    read_primitive(in, result.line_join);
  }

  if (vl.version >= 4) {
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
  result.dx_array.resize(dx_array_length);
  for (std::uint32_t i = 0; i < dx_array_length; ++i) {
    read_primitive(in, result.dx_array[i]);
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
