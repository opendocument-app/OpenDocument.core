#include <odr/internal/svm/svm_format.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace odr::internal {

namespace {

/// Reads exactly @p size bytes, growing the buffer as the data arrives so that
/// a bogus length prefix cannot allocate ahead of the stream.
std::string read_bytes(std::istream &in, const std::uint64_t size) {
  constexpr std::uint64_t chunk_size = 4096;

  std::string result;
  while (result.size() < size) {
    const std::size_t offset = result.size();
    const auto step =
        static_cast<std::size_t>(std::min(chunk_size, size - offset));
    result.resize(offset + step);
    if (!in.read(result.data() + offset, static_cast<std::streamsize>(step))) {
      throw MalformedSvmFile();
    }
  }
  return result;
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
