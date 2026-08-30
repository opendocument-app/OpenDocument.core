#include <odr/internal/svm/svm_format.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/png_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <utility>

namespace odr::internal {

namespace {

std::string read_bytes(std::istream &in, const std::uint64_t size) {
  try {
    return util::byte_stream::read_u8s(in, size);
  } catch (const std::runtime_error &) {
    throw MalformedSvmFile();
  }
}

/// `RegionType`, what shape the region was streamed as.
constexpr std::uint16_t region_null = 0;
constexpr std::uint16_t region_empty = 1;
/// `StreamEntryType`, what the band list holds.
constexpr std::uint16_t band_header = 0;
constexpr std::uint16_t band_separation = 1;
constexpr std::uint16_t band_end = 2;

/// A `.bmp` starts with `"BM"`; `"BA"`, an os/2 bitmap array, does not.
constexpr std::uint16_t bmp_magic = 0x4d42;
constexpr std::uint32_t bmp_file_header_size = 14;
constexpr std::uint32_t dib_core_header_size = 12;
/// `ZCOMPRESS` - LibreOffice's own, a zlib stream where the pixels go.
constexpr std::uint32_t dib_zcompress = ('S' | ('D' << 8)) | 0x01000000;
/// `ReadDIBBitmapEx`: what marks the transparency data behind a dib.
constexpr std::uint32_t bitmap_ex_magic_1 = 0x25091962;
constexpr std::uint32_t bitmap_ex_magic_2 = 0xacb20201;
/// `TransparentType::Bitmap`, the only one that carries a second dib.
constexpr std::uint8_t bitmap_ex_mask = 2;

std::uint16_t read_u16(const std::string &bytes, const std::size_t offset) {
  std::uint16_t result{};
  std::memcpy(&result, bytes.data() + offset, sizeof(result));
  return result;
}

std::uint32_t read_u32(const std::string &bytes, const std::size_t offset) {
  std::uint32_t result{};
  std::memcpy(&result, bytes.data() + offset, sizeof(result));
  return result;
}

/// As much of a dib's header as unpacking its pixels needs.
struct DibLayout final {
  std::uint32_t off_bits{};
  std::uint32_t header_size{};
  std::int32_t width{};
  /// As the header states it: negative for a top-down dib.
  std::int32_t height{};
  std::uint32_t bit_count{};
  std::uint32_t compression{};
};

/// The dib's pixels as 8-bit rgb rows, top to bottom - a png's layout. Empty
/// where the dib is compressed, bit-fielded, or 16 bits a pixel.
std::string get_rgb_rows(const std::string &bmp, const DibLayout &layout) {
  if (layout.compression != 0 || layout.width <= 0 || layout.height == 0) {
    return {};
  }
  if (layout.bit_count != 32 && layout.bit_count != 24 &&
      layout.bit_count != 8 && layout.bit_count != 4 && layout.bit_count != 1) {
    return {};
  }

  const auto width = static_cast<std::size_t>(layout.width);
  const auto height = static_cast<std::size_t>(std::abs(layout.height));
  const std::size_t stride = ((width * layout.bit_count + 31) / 32) * 4;
  if (bmp.size() < layout.off_bits + stride * height) {
    return {};
  }

  // a palette sits between the header and the pixels, three bytes an entry in
  // the core header's dib and four in every later one
  const std::size_t entry_size =
      layout.header_size == dib_core_header_size ? 3 : sizeof(std::uint32_t);
  const std::size_t palette_offset = bmp_file_header_size + layout.header_size;
  const std::size_t palette_entries =
      layout.off_bits > palette_offset
          ? (layout.off_bits - palette_offset) / entry_size
          : 0;
  if (layout.bit_count <= 8 && palette_entries == 0) {
    return {};
  }

  std::string rows(width * height * 3, '\0');
  for (std::size_t y = 0; y < height; ++y) {
    // rows run bottom-up unless the height says otherwise
    const std::size_t source = layout.height < 0 ? y : height - 1 - y;
    const char *pixels = bmp.data() + layout.off_bits + source * stride;
    char *target = rows.data() + y * width * 3;

    for (std::size_t x = 0; x < width; ++x) {
      const auto *bgr = reinterpret_cast<const std::uint8_t *>(pixels);
      std::size_t index = 0;

      switch (layout.bit_count) {
      case 32:
      case 24: {
        const std::size_t at = x * (layout.bit_count / 8);
        target[x * 3 + 0] = static_cast<char>(bgr[at + 2]);
        target[x * 3 + 1] = static_cast<char>(bgr[at + 1]);
        target[x * 3 + 2] = static_cast<char>(bgr[at + 0]);
        continue;
      }
      case 8:
        index = bgr[x];
        break;
      case 4:
        index = (bgr[x / 2] >> (x % 2 == 0 ? 4 : 0)) & 0x0f;
        break;
      default:
        index = (bgr[x / 8] >> (7 - x % 8)) & 0x01;
        break;
      }

      if (index >= palette_entries) {
        return {};
      }
      const auto *entry = reinterpret_cast<const std::uint8_t *>(
          bmp.data() + palette_offset + index * entry_size);
      target[x * 3 + 0] = static_cast<char>(entry[2]);
      target[x * 3 + 1] = static_cast<char>(entry[1]);
      target[x * 3 + 2] = static_cast<char>(entry[0]);
    }
  }

  return rows;
}

/// `DrawText(…, index, len)`: a text action names the run of its string that
/// it draws, and a length past the end means the rest of it.
template <typename String>
String select_run(const String &text, const std::uint16_t offset,
                  const std::uint16_t length) {
  if (offset >= text.size()) {
    return {};
  }
  return text.substr(offset, length);
}

/// @ref select_run in the units the run is measured in - utf-16 code units for
/// `UCS2`, bytes otherwise. @p text has already been decoded to utf-8, where a
/// `UCS2` offset addresses nothing and a `substr` splits a character.
std::string select_run_with_encoding(const std::string &text,
                                     const svm::TextEncoding encoding,
                                     const std::uint16_t offset,
                                     const std::uint16_t length) {
  if (encoding != svm::RTL_TEXTENCODING_UCS2) {
    return select_run(text, offset, length);
  }
  return util::string::u16string_to_string(
      select_run(util::string::string_to_u16string(text), offset, length));
}

std::u16string read_u16string(std::istream &in, const std::uint32_t length) {
  const std::string bytes =
      read_bytes(in, static_cast<std::uint64_t>(length) * 2);
  std::u16string result(length, u' ');
  std::memcpy(result.data(), bytes.data(), bytes.size());
  return result;
}

} // namespace

std::string svm::read_ascii_string(std::istream &in,
                                   const std::uint32_t length) {
  return read_bytes(in, length);
}

std::string svm::read_utf16_string(std::istream &in,
                                   const std::uint32_t length) {
  return util::string::u16string_to_string(read_u16string(in, length));
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

std::u16string svm::read_uint16_prefixed_u16string(std::istream &in) {
  std::uint16_t length;
  read_primitive(in, length);
  return read_u16string(in, length);
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

  // every coordinate divides by these
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

svm::PixelAction svm::read_pixel_action(std::istream &in) {
  PixelAction result;

  result.point = read_int_pair(in);
  read_primitive(in, result.color);

  return result;
}

svm::LineAction svm::read_line_action(std::istream &in,
                                      const VersionLength &vl) {
  LineAction result;

  result.start = read_int_pair(in);
  result.end = read_int_pair(in);

  if (vl.version >= 2) {
    result.line_info = read_line_info(in);
  }

  return result;
}

svm::RoundRectangleAction svm::read_round_rectangle_action(std::istream &in) {
  RoundRectangleAction result;

  result.rectangle = read_rectangle(in);
  read_primitive(in, result.horizontal_round);
  read_primitive(in, result.vertical_round);

  return result;
}

svm::ArcAction svm::read_arc_action(std::istream &in) {
  ArcAction result;

  result.rectangle = read_rectangle(in);
  result.start = read_int_pair(in);
  result.end = read_int_pair(in);

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
    result.text = util::string::u16string_to_string(select_run(
        read_uint16_prefixed_u16string(in), result.offset, result.length));
  } else {
    result.text = select_run_with_encoding(result.text, encoding, result.offset,
                                           result.length);
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
    result.text = util::string::u16string_to_string(select_run(
        read_uint16_prefixed_u16string(in), result.offset, result.length));
  } else {
    result.text = select_run_with_encoding(result.text, encoding, result.offset,
                                           result.length);
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
    result.text = util::string::u16string_to_string(select_run(
        read_uint16_prefixed_u16string(in), result.offset, result.length));
  } else {
    result.text = select_run_with_encoding(result.text, encoding, result.offset,
                                           result.length);
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

svm::Image svm::read_dib(std::istream &in, const std::uint32_t limit) {
  Image result;
  DibLayout layout;

  std::string bytes = read_bytes(in, bmp_file_header_size);
  if (read_u16(bytes, 0) != bmp_magic) {
    throw MalformedSvmFile();
  }
  layout.off_bits = read_u32(bytes, 10);

  bytes += read_bytes(in, sizeof(std::uint32_t));
  layout.header_size = read_u32(bytes, bmp_file_header_size);
  if (layout.header_size < dib_core_header_size || layout.header_size > limit) {
    throw MalformedSvmFile();
  }
  bytes += read_bytes(in, layout.header_size - sizeof(std::uint32_t));

  std::uint32_t size_image{};
  if (layout.header_size == dib_core_header_size) {
    layout.width = read_u16(bytes, 18);
    layout.height = read_u16(bytes, 20);
    layout.bit_count = read_u16(bytes, 24);
  } else {
    layout.width = static_cast<std::int32_t>(read_u32(bytes, 18));
    layout.height = static_cast<std::int32_t>(read_u32(bytes, 22));
    layout.bit_count = read_u16(bytes, 28);
    layout.compression = read_u32(bytes, 30);
    size_image = read_u32(bytes, 34);
  }
  // a negative height is a top-down dib; it is still that many rows
  result.size_pixel = {layout.width, std::abs(layout.height)};

  if (layout.compression == dib_zcompress) {
    // the palette and the pixels are inside the stream, so nothing but its
    // length can be read without inflating it
    const std::string prefix = read_bytes(in, 3 * sizeof(std::uint32_t));
    read_bytes(in, read_u32(prefix, 0));
    return result;
  }

  if (layout.off_bits < bytes.size() || layout.off_bits > limit) {
    throw MalformedSvmFile();
  }
  // the palette, and any gap the writer left before the pixels
  bytes += read_bytes(in, layout.off_bits - bytes.size());

  // `bfSize` is written from the uncompressed size, so it says nothing about a
  // compressed dib; the header's own numbers do
  const auto stride =
      ((static_cast<std::uint64_t>(result.size_pixel.x) * layout.bit_count +
        31) /
       32) *
      4;
  const std::uint64_t pixels = layout.compression == 0
                                   ? stride * result.size_pixel.y
                                   : static_cast<std::uint64_t>(size_image);
  if (pixels == 0 || pixels > limit) {
    throw MalformedSvmFile();
  }
  bytes += read_bytes(in, pixels);

  if (const std::string rows = get_rgb_rows(bytes, layout); !rows.empty()) {
    result.data =
        util::png::write(rows, result.size_pixel.x, result.size_pixel.y, 3);
    result.mime_type = "image/png";
  }
  if (result.data.empty()) {
    result.data = std::move(bytes);
    result.mime_type = "image/bmp";
  }
  return result;
}

svm::Bitmap svm::read_dib_bitmap_ex(std::istream &in,
                                    const std::uint32_t limit) {
  Bitmap result;
  result.image = read_dib(in, limit);

  // the transparency data is optional, so what is not it is put back
  const std::istream::pos_type position = in.tellg();
  std::uint32_t magic_1{};
  std::uint32_t magic_2{};
  read_primitive(in, magic_1);
  read_primitive(in, magic_2);
  if (magic_1 != bitmap_ex_magic_1 || magic_2 != bitmap_ex_magic_2) {
    in.seekg(position);
    return result;
  }

  std::uint8_t type{};
  read_primitive(in, type);
  if (type == bitmap_ex_mask) {
    result.mask = read_dib(in, limit);
  }

  return result;
}

svm::BitmapAction svm::read_bitmap_action(std::istream &in,
                                          const std::uint16_t type,
                                          const VersionLength &vl) {
  BitmapAction result;

  const bool with_mask = type == META_BMPEX_ACTION ||
                         type == META_BMPEXSCALE_ACTION ||
                         type == META_BMPEXSCALEPART_ACTION;
  if (with_mask) {
    result.bitmap = read_dib_bitmap_ex(in, vl.length);
  } else {
    result.bitmap.image = read_dib(in, vl.length);
  }

  result.point = read_int_pair(in);

  if (type == META_BMPSCALE_ACTION || type == META_BMPEXSCALE_ACTION) {
    result.size = read_int_pair(in);
  } else if (type == META_BMPSCALEPART_ACTION ||
             type == META_BMPEXSCALEPART_ACTION) {
    result.size = read_int_pair(in);
    result.source_point = read_int_pair(in);
    result.source_size = read_int_pair(in);
  }

  return result;
}

std::optional<svm::Region> svm::read_region(std::istream &in) {
  const VersionLength vl = read_version_length(in);
  std::uint16_t content_version{};
  std::uint16_t type{};
  read_primitive(in, content_version);
  read_primitive(in, type);

  if (type == region_null) {
    return std::nullopt;
  }

  Region result;
  if (type == region_empty) {
    return result;
  }

  // horizontal strips, each with the spans inside the region
  std::int32_t top{};
  std::int32_t bottom{};
  while (true) {
    std::uint16_t entry{};
    read_primitive(in, entry);
    if (entry == band_end) {
      break;
    }

    std::int32_t first{};
    std::int32_t second{};
    read_primitive(in, first);
    read_primitive(in, second);

    if (entry == band_header) {
      top = first;
      bottom = second;
    } else if (entry == band_separation) {
      result.rectangles.push_back({first, top, second, bottom});
    } else {
      throw MalformedSvmFile();
    }
  }

  if (vl.version >= 2) {
    bool has_polygons{};
    read_primitive(in, has_polygons);
    if (has_polygons) {
      result.polygons = read_poly_polygon(in);
    }
  }

  return result;
}

std::pair<std::optional<svm::Region>, bool>
svm::read_clip_region_action(std::istream &in) {
  std::optional<Region> region = read_region(in);
  bool clip{};
  read_primitive(in, clip);
  return {std::move(region), clip};
}

std::uint16_t svm::read_text_align_action(std::istream &in) {
  std::uint16_t result;
  read_primitive(in, result);
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
