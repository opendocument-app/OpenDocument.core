#include <codecvt>
#include <cstring>
#include <glog/logging.h>
#include <locale>
#include <string>
#include <svm/Svm2Svg.h>
#include <vector>

namespace odr {
namespace svm {

namespace {
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

template <typename T> void readPrimitive(std::istream &in, T &out) {
  in.read((char *)&out, sizeof(out));
}

std::string readAsciiString(std::istream &in, const std::uint32_t length) {
  std::string result(length, ' ');
  in.read((char *)result.data(), result.size());
  return result;
}

std::string readUtf16String(std::istream &in, const std::uint32_t length) {
  std::u16string resultU16(length, ' ');
  in.read((char *)resultU16.data(), length * 2);
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conversion;
  return conversion.to_bytes(resultU16);
}

std::string readUint16PrefixedAsciiString(std::istream &in) {
  uint16_t length;
  readPrimitive(in, length);
  return readAsciiString(in, length);
}

std::string readUint32PrefixedUtf16String(std::istream &in) {
  uint32_t length;
  readPrimitive(in, length);
  return readUtf16String(in, length);
}

std::string readUint16PrefixedUtf16String(std::istream &in) {
  uint16_t length;
  readPrimitive(in, length);
  return readUtf16String(in, length);
}

std::string readStringWithEncoding(std::istream &in,
                                   const TextEncoding encoding) {
  if (encoding == RTL_TEXTENCODING_UCS2) {
    return readUint32PrefixedUtf16String(in);
  }
  return readUint16PrefixedAsciiString(in);
}

struct VersionLength final {
  std::uint16_t version{};
  std::uint32_t length{};

  void read(std::istream &in) {
    readPrimitive(in, version);
    readPrimitive(in, length);
    if (version <= 0) {
      DLOG(WARNING) << "VersionLength illegal version";
    }
  }
};

struct IntPair final {
  std::int32_t x{};
  std::int32_t y{};

  void read(std::istream &in) {
    readPrimitive(in, x);
    readPrimitive(in, y);
  }
};

struct Rectangle final {
  std::int32_t left{};
  std::int32_t top{};
  std::int32_t right{};
  std::int32_t bottom{};

  void read(std::istream &in) {
    readPrimitive(in, left);
    readPrimitive(in, top);
    readPrimitive(in, right);
    readPrimitive(in, bottom);
  }
};

std::vector<IntPair> readPolygon(std::istream &in) {
  std::vector<IntPair> result;

  std::uint16_t size;
  readPrimitive(in, size);

  result.resize(size);
  for (auto &&p : result) {
    p.read(in);
  }

  return result;
}

std::vector<std::vector<IntPair>> readPolyPolygon(std::istream &in) {
  std::vector<std::vector<IntPair>> result;

  std::uint16_t size;
  readPrimitive(in, size);

  result.resize(size);
  for (auto &&p : result) {
    p = readPolygon(in);
  }

  return result;
}

struct MapMode final {
  std::uint16_t unit{};
  IntPair origin;
  IntPair scale_x;
  IntPair scale_y;
  bool simple{};

  void read(std::istream &in) {
    VersionLength vl;
    vl.read(in);

    readPrimitive(in, unit);
    origin.read(in);
    scale_x.read(in);
    scale_y.read(in);
    readPrimitive(in, simple);
  }
};

struct LineInfo final {
  std::uint16_t lineStyle{};
  std::int32_t width{};

  std::uint16_t dashCount{};
  std::int32_t dashLength{};
  std::uint16_t dotCount{};
  std::int32_t dotLength{};
  std::int32_t distance{};

  std::uint16_t lineJoin{};

  void read(std::istream &in) {
    VersionLength vl;
    vl.read(in);

    readPrimitive(in, lineStyle);
    readPrimitive(in, width);

    if (vl.version >= 2) {
      readPrimitive(in, dashCount);
      readPrimitive(in, dashLength);
      readPrimitive(in, dotCount);
      readPrimitive(in, dotLength);
      readPrimitive(in, distance);
    }

    if (vl.version >= 3) {
      readPrimitive(in, lineJoin);
    }

    if (vl.version >= 4) {
      // TODO
      DLOG(WARNING) << "LineInfo version 4 not implemented";
    }
  }
};

struct Font final {
  VersionLength vl;
  std::string familyName;
  std::string styleName;
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
  std::uint16_t cjkLanguage{};
  bool vertical{};
  std::uint16_t emphasisMark{};

  // version 3
  std::uint16_t overline{};

  void read(std::istream &in) {
    vl.read(in);
    familyName = readUint16PrefixedAsciiString(in);
    styleName = readUint16PrefixedAsciiString(in);
    size.read(in);
    readPrimitive(in, charset);
    readPrimitive(in, family);
    readPrimitive(in, pitch);
    readPrimitive(in, weight);
    readPrimitive(in, underline);
    readPrimitive(in, strikeout);
    readPrimitive(in, italic);
    readPrimitive(in, language);
    readPrimitive(in, width);
    readPrimitive(in, orientation);
    readPrimitive(in, wordline);
    readPrimitive(in, outline);
    readPrimitive(in, shadow);
    readPrimitive(in, kerning);

    if (vl.version >= 2) {
      readPrimitive(in, relief);
      readPrimitive(in, cjkLanguage);
      readPrimitive(in, vertical);
      readPrimitive(in, emphasisMark);
    }

    if (vl.version >= 3) {
      readPrimitive(in, overline);
    }
  }
};

struct Header final {
  VersionLength vl;
  std::uint32_t compression_mode{};
  MapMode map_mode;
  IntPair size;
  std::uint32_t action_count{};
  std::uint8_t render_graphic_replacements{};

  void read(std::istream &in) {
    char magic[6];
    in.read(magic, sizeof(magic));
    if (std::strncmp("VCLMTF", magic, sizeof(magic)) != 0) {
      throw NoSvmFileException();
    }
    vl.read(in);
    std::size_t start = in.tellg();
    readPrimitive(in, compression_mode);
    map_mode.read(in);
    size.read(in);
    readPrimitive(in, action_count);
    if (vl.version >= 2) {
      readPrimitive(in, render_graphic_replacements);
    }
    std::size_t left = vl.length - ((std::size_t)in.tellg() - start);
    if (left > 0) {
      DLOG(WARNING) << "Header skipping " << left << " bytes";
      in.ignore(left);
    }
  }
};

struct ActionHeader final {
  std::uint16_t type{};
  VersionLength vl;

  void read(std::istream &in) {
    readPrimitive(in, type);
    vl.read(in);
  }
};

struct PolyLineAction final {
  std::vector<IntPair> points;
  LineInfo lineInfo;

  void read(std::istream &in, const VersionLength &vl) {
    points = readPolygon(in);

    if (vl.version >= 2) {
      lineInfo.read(in);
    }

    if (vl.version >= 3) {
      bool hasFlags;
      readPrimitive(in, hasFlags);

      if (hasFlags) {
        // TODO
        DLOG(WARNING) << "PolyLineAction flags not implemented";
      }
    }
  }
};

struct PolygonAction final {
  std::vector<IntPair> points;

  void read(std::istream &in, const VersionLength &vl) {
    points = readPolygon(in);

    if (vl.version >= 3) {
      bool hasFlags;
      readPrimitive(in, hasFlags);

      if (hasFlags) {
        // TODO
        DLOG(WARNING) << "PolygonAction flags not implemented";
      }
    }
  }
};

struct PolyPolygonAction final {
  std::vector<std::vector<IntPair>> polygons;

  void read(std::istream &in, const VersionLength &vl) {
    polygons = readPolyPolygon(in);

    if (vl.version >= 2) {
      std::uint16_t complexPolygons;
      readPrimitive(in, complexPolygons);

      if (complexPolygons > 0) {
        // TODO
        DLOG(WARNING) << "PolyPolygonAction complex not implemented";
      }
    }
  }
};

struct TextAction final {
  IntPair point;
  std::string text;
  std::uint16_t offset{};
  std::uint16_t length{};

  void read(std::istream &in, const VersionLength &vl,
            const TextEncoding encoding) {
    point.read(in);
    text = readStringWithEncoding(in, encoding);
    readPrimitive(in, offset);
    readPrimitive(in, length);

    if (vl.version >= 2) {
      text = readUint16PrefixedUtf16String(in);
    }
  }
};

struct TextArrayAction final {
  IntPair point;
  std::string text;
  std::uint16_t offset{};
  std::uint16_t length{};
  std::vector<std::uint32_t> dxArray;

  void read(std::istream &in, const VersionLength &vl,
            const TextEncoding encoding) {
    point.read(in);
    text = readStringWithEncoding(in, encoding);
    readPrimitive(in, offset);
    readPrimitive(in, length);
    std::uint32_t dxArrayLength;
    readPrimitive(in, dxArrayLength);
    dxArray.resize(dxArrayLength);
    for (std::uint32_t i = 0; i < dxArrayLength; ++i) {
      readPrimitive(in, dxArray[i]);
    }

    if (vl.version >= 2) {
      text = readUint16PrefixedUtf16String(in);
    }
  }
};

struct StretchTextAction final {
  IntPair point;
  std::string text;
  std::uint32_t width{};
  std::uint16_t offset{};
  std::uint16_t length{};

  void read(std::istream &in, const VersionLength &vl,
            const TextEncoding encoding) {
    point.read(in);
    text = readStringWithEncoding(in, encoding);
    readPrimitive(in, width);
    readPrimitive(in, offset);
    readPrimitive(in, length);

    if (vl.version >= 2) {
      text = readUint16PrefixedUtf16String(in);
    }
  }
};

struct TextRectangleAction final {
  Rectangle rectangle;
  std::string text;
  std::uint16_t style{};

  void read(std::istream &in, const VersionLength &vl,
            const TextEncoding encoding) {
    rectangle.read(in);
    text = readStringWithEncoding(in, encoding);
    readPrimitive(in, style);

    if (vl.version >= 2) {
      text = readUint16PrefixedUtf16String(in);
    }
  }
};

struct TextLineAction final {
  IntPair rectangle;
  std::int32_t width{};
  std::uint32_t strikeout{};
  std::uint32_t underline{};
  std::uint32_t overline{};

  void read(std::istream &in, const VersionLength &vl) {
    rectangle.read(in);
    readPrimitive(in, width);
    readPrimitive(in, strikeout);
    readPrimitive(in, underline);

    if (vl.version >= 2) {
      readPrimitive(in, overline);
    }
  }
};

struct SvmContext final {
  std::istream *in{};
  std::ostream *out{};
  const ActionHeader *action{};

  MapMode mapMode;
  TextEncoding encoding{};
  Font font;
  TextLineAction textLine;
  std::uint32_t fillRGB{};
  bool fillRGBSet{};
  std::uint32_t lineRGB{};
  bool lineRGBSet{};
  std::uint32_t overLineRGB{};
  std::uint32_t textRGB{};
  std::uint32_t textFillRGB{};
  bool textFillRGBSet{};
};

std::string getSVGColorString(const std::uint32_t color) {
  const uint8_t blue = (color >> 0) & 0xFF;
  const uint8_t green = (color >> 8) & 0xFF;
  const uint8_t red = (color >> 16) & 0xFF;
  return "rgb(" + std::to_string(red) + "," + std::to_string(green) + "," +
         std::to_string(blue) + ")";
}

void writeColorStyle(std::ostream &out, const std::string &prefix,
                     const std::uint32_t color, const bool set) {
  if (set) {
    out << prefix << ":" << getSVGColorString(color);
  } else {
    out << prefix << "-opacity:0";
  }
  out << ";";
}

void writeLineStyle(std::ostream &out, SvmContext &context) {
  writeColorStyle(out, "stroke", context.lineRGB, context.lineRGBSet);
  out << "vector-effect:non-scaling-stroke;";
  out << "fill:none;";
}

void writeFillStyle(std::ostream &out, SvmContext &context) {
  writeColorStyle(out, "fill", context.fillRGB, context.fillRGBSet);
  out << "stroke:none;";
}

void writeTextStyle(std::ostream &out, SvmContext &context) {
  writeColorStyle(out, "fill", context.textRGB, true);
  out << "font-family:" << context.font.familyName << ";";
  out << "font-size:" << context.font.size.y << ";";
}

void writeStyle(std::ostream &out, SvmContext &context, const int styles) {
  out << " style=\"";
  switch (styles) {
  case 0:
    writeLineStyle(out, context);
    break;
  case 1:
    writeLineStyle(out, context);
    writeFillStyle(out, context);
    break;
  case 2:
    writeTextStyle(out, context);
    break;
  default:
    DLOG(WARNING) << "not implemented";
  }
  out << "\"";
}

void writeRectangle(std::ostream &out, const Rectangle &rect,
                    SvmContext &context) {
  out << "<rect";
  out << " x=\"" << rect.left << "\"";
  out << " y=\"" << rect.top << "\"";
  out << " width=\"" << (rect.right - rect.left) << "\"";
  out << " height=\"" << (rect.bottom - rect.top) << "\"";
  writeStyle(out, context, 1);
  out << " />";
}

void writePolygon(std::ostream &out, const std::string &tag,
                  const std::vector<IntPair> &points, const bool fill,
                  SvmContext &context) {
  out << "<" << tag;

  out << " points=\"";
  for (auto &&p : points) {
    out << p.x << "," << p.y;
    out << " ";
  }
  out << "\"";

  if (fill) {
    writeStyle(out, context, 1);
  } else {
    writeStyle(out, context, 0);
  }

  out << " />";
}

void writeText(std::ostream &out, const IntPair &point, const std::string &text,
               SvmContext &context) {
  out << "<text";
  out << " x=\"" << point.x << "\"";
  out << " y=\"" << point.y << "\"";
  writeStyle(out, context, 2);
  out << ">";
  out << text;
  out << "</text>";
}

void translateAction(const ActionHeader &action, std::istream &in,
                     std::ostream &out, SvmContext &context) {
  switch (action.type) {
  case META_FILLCOLOR_ACTION:
    readPrimitive(in, context.fillRGB);
    readPrimitive(in, context.fillRGBSet);
    break;
  case META_LINECOLOR_ACTION:
    readPrimitive(in, context.lineRGB);
    readPrimitive(in, context.lineRGBSet);
    break;
  case META_OVERLINECOLOR_ACTION:
    readPrimitive(in, context.overLineRGB);
    break;
  case META_TEXTCOLOR_ACTION:
    readPrimitive(in, context.textRGB);
    break;
  case META_TEXTFILLCOLOR_ACTION:
    readPrimitive(in, context.textFillRGB);
    readPrimitive(in, context.textFillRGBSet);
    break;
  case META_FONT_ACTION:
    context.font.read(in);
    context.encoding = (TextEncoding)context.font.charset;
    break;
  case META_TEXTLINE_ACTION:
    context.textLine.read(in, action.vl);
    break;
  case META_RECT_ACTION: {
    Rectangle action2;
    action2.read(in);
    writeRectangle(out, action2, context);
  } break;
  case META_POLYLINE_ACTION: {
    PolyLineAction action2;
    action2.read(in, action.vl);
    writePolygon(out, "polyline", action2.points, false, context);
  } break;
  case META_POLYGON_ACTION: {
    PolygonAction action2;
    action2.read(in, action.vl);
    writePolygon(out, "polygon", action2.points, true, context);
  } break;
  case META_POLYPOLYGON_ACTION: {
    PolyPolygonAction action2;
    action2.read(in, action.vl);

    for (auto &&p : action2.polygons) {
      writePolygon(out, "polygon", p, true, context);
    }
  } break;
  case META_TEXT_ACTION: {
    TextAction action2;
    action2.read(in, action.vl, context.encoding);
    writeText(out, action2.point, action2.text, context);
  } break;
  case META_TEXTARRAY_ACTION: {
    TextArrayAction action2;
    action2.read(in, action.vl, context.encoding);
    writeText(out, action2.point, action2.text, context);
  } break;
  case META_STRETCHTEXT_ACTION: {
    StretchTextAction action2;
    action2.read(in, action.vl, context.encoding);
    writeText(out, action2.point, action2.text, context);
  } break;
  case META_TEXTRECT_ACTION: {
    TextRectangleAction action2;
    // action2.read(in, action.vl, context.encoding);
    // TODO
  } break;
  case META_NULL_ACTION:
  case META_PUSH_ACTION:
  case META_POP_ACTION:
  case META_TEXTLANGUAGE_ACTION:
  case META_COMMENT_ACTION:
    in.ignore(action.vl.length);
    break;
  default:
    DLOG(WARNING) << "unhandled action " << action.type << " version "
                  << action.vl.version << " length " << action.vl.length;
    in.ignore(action.vl.length);
    break;
  }
}
} // namespace

void Translator::svg(std::istream &in, std::ostream &out) {
  SvmContext context{};
  context.in = &in;
  context.out = &out;

  Header header{};
  header.read(in);

  context.encoding = RTL_TEXTENCODING_ASCII_US;
  context.mapMode = header.map_mode;

  out << "<svg";
  out << " xmlns=\"http://www.w3.org/2000/svg\"";
  out << " version=\"1.1\"";
  out << " viewBox=\"0 0 " << header.size.x << " " << header.size.y << "\"";
  out << ">";

  while (in.peek() != -1) {
    // TODO check length fields should never exceed file size (limited
    // inputstream?)
    ActionHeader action{};
    action.read(in);
    std::int64_t start = in.tellg();

    translateAction(action, in, out, context);

    const std::int64_t left =
        action.vl.length - ((std::int64_t)in.tellg() - start);
    if (left > 0) {
      DLOG(WARNING) << "skipping " << left << " bytes of action " << action.type
                    << " version " << action.vl.version;
      in.ignore(left);
    } else if (left < 0) {
      LOG(ERROR) << -left << " bytes missing action " << action.type
                 << " version " << action.vl.version;
      throw MalformedSvmFileException();
    }
  }

  out << "</svg>";
}

} // namespace svm
} // namespace odr
