#include <odr/internal/pdf/pdf_annotation.hpp>

#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_writer.hpp>
#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include <fmt/format.h>

namespace odr::internal::pdf {

namespace {

/// A rectangle in user space, as `/Rect` and `/BBox` state it.
struct Box {
  double x0{0};
  double y0{0};
  double x1{0};
  double y1{0};

  void include(const double x, const double y) {
    x0 = std::min(x0, x);
    y0 = std::min(y0, y);
    x1 = std::max(x1, x);
    y1 = std::max(y1, y);
  }
  [[nodiscard]] Box grown(const double margin) const {
    return {x0 - margin, y0 - margin, x1 + margin, y1 + margin};
  }
};

Box box_of(const std::vector<Quad> &quads) {
  Box result{quads.front()[0], quads.front()[1], quads.front()[0],
             quads.front()[1]};
  for (const Quad &quad : quads) {
    for (std::size_t i = 0; i < quad.size(); i += 2) {
      result.include(quad[i], quad[i + 1]);
    }
  }
  return result;
}

Box box_of(const std::vector<InkStroke> &strokes) {
  Box result{strokes.front()[0], strokes.front()[1], strokes.front()[0],
             strokes.front()[1]};
  for (const InkStroke &stroke : strokes) {
    for (std::size_t i = 0; i < stroke.size(); i += 2) {
      result.include(stroke[i], stroke[i + 1]);
    }
  }
  return result;
}

Object rectangle(const Box &box) {
  Array result;
  for (const double v : {box.x0, box.y0, box.x1, box.y1}) {
    result.holder().emplace_back(Real{v});
  }
  return Object(std::move(result));
}

std::string number(const double value) {
  return util::number::to_string_significant(value, 6);
}

/// The `/ExtGState` an appearance needs, and the `gs` name to invoke it with.
/// `blend` is empty for the default Normal.
Dictionary graphics_state_resources(const double alpha,
                                    const std::string_view blend) {
  Dictionary state;
  state["ca"] = Object(Real{alpha});
  state["CA"] = Object(Real{alpha});
  if (!blend.empty()) {
    state["BM"] = Object(Name{std::string(blend)});
  }
  Dictionary states;
  states["G0"] = Object(std::move(state));
  Dictionary result;
  result["ExtGState"] = Object(std::move(states));
  return result;
}

/// The form XObject an annotation's `/AP /N` points at. A transparency group
/// is what lets `/BM /Multiply` composite against the page rather than against
/// the form's own backdrop.
Dictionary appearance_dictionary(const Box &box, Dictionary resources,
                                 const bool transparency_group) {
  Dictionary result;
  result["Type"] = Object(Name{"XObject"});
  result["Subtype"] = Object(Name{"Form"});
  result["BBox"] = rectangle(box);
  result["Resources"] = Object(std::move(resources));
  if (transparency_group) {
    Dictionary group;
    group["Type"] = Object(Name{"Group"});
    group["S"] = Object(Name{"Transparency"});
    group["CS"] = Object(Name{"DeviceRGB"});
    result["Group"] = Object(std::move(group));
  }
  return result;
}

void write_common(Dictionary &dictionary, const AnnotationCommon &common,
                  const ObjectReference &self) {
  Array color;
  for (const double c : common.color) {
    color.holder().emplace_back(Real{c});
  }
  dictionary["C"] = Object(std::move(color));
  dictionary["CA"] = Object(Real{common.opacity});
  // 12.5.3: bit 3, Print. Without it a viewer may show but never print it.
  dictionary["F"] = Object(Integer{4});
  // unique within the file, and stable for a given file and annotation
  dictionary["NM"] = Object(StandardString{fmt::format("odr-{}", self.id)});
  if (!common.author.empty()) {
    dictionary["T"] = Object(StandardString{common.author});
  }
  if (!common.contents.empty()) {
    dictionary["Contents"] = Object(StandardString{common.contents});
  }
}

std::string_view subtype_of(const TextMarkupKind kind) {
  switch (kind) {
  case TextMarkupKind::highlight:
    return "Highlight";
  case TextMarkupKind::underline:
    return "Underline";
  case TextMarkupKind::strike_out:
    return "StrikeOut";
  case TextMarkupKind::squiggly:
    return "Squiggly";
  }
  throw std::invalid_argument("unknown text markup kind");
}

/// A quad's corners, named. The order is the one `Quad` documents.
struct QuadCorners {
  double left{0};
  double right{0};
  double top{0};
  double bottom{0};
};

QuadCorners corners_of(const Quad &quad) {
  return {std::min({quad[0], quad[2], quad[4], quad[6]}),
          std::max({quad[0], quad[2], quad[4], quad[6]}),
          std::max({quad[1], quad[3], quad[5], quad[7]}),
          std::min({quad[1], quad[3], quad[5], quad[7]})};
}

/// A filled bar across `quad`, `height` tall, its bottom at `bottom`.
void bar(std::ostringstream &out, const QuadCorners &quad, const double bottom,
         const double height) {
  out << number(quad.left) << ' ' << number(bottom) << ' '
      << number(quad.right - quad.left) << ' ' << number(height) << " re\n";
}

/// A wave along the bottom of `quad`, as a stroked zigzag of `amplitude`.
void wave(std::ostringstream &out, const QuadCorners &quad,
          const double amplitude) {
  const double base = quad.bottom + amplitude;
  out << number(quad.left) << ' ' << number(base) << " m\n";
  const auto steps = static_cast<std::size_t>(
      std::max(1.0, std::floor((quad.right - quad.left) / amplitude)));
  for (std::size_t i = 1; i <= steps; ++i) {
    out << number(quad.left + static_cast<double>(i) * amplitude) << ' '
        << number(i % 2 == 1 ? base + amplitude : base) << " l\n";
  }
  out << "S\n";
}

std::string text_markup_appearance(const TextMarkup &markup) {
  std::ostringstream out;
  out << "/G0 gs\n";
  const auto &[r, g, b] = markup.common.color;
  out << number(r) << ' ' << number(g) << ' ' << number(b);

  if (markup.kind == TextMarkupKind::squiggly) {
    out << " RG\n";
  } else {
    out << " rg\n";
  }

  for (const Quad &quad : markup.quads) {
    const QuadCorners c = corners_of(quad);
    const double height = c.top - c.bottom;
    switch (markup.kind) {
    case TextMarkupKind::highlight:
      bar(out, c, c.bottom, height);
      break;
    case TextMarkupKind::underline:
      bar(out, c, c.bottom + height / 16, std::max(height / 16, 0.5));
      break;
    case TextMarkupKind::strike_out:
      bar(out, c, c.bottom + height / 2, std::max(height / 16, 0.5));
      break;
    case TextMarkupKind::squiggly:
      out << number(std::max(height / 16, 0.5)) << " w\n";
      wave(out, c, std::max(height / 8, 1.0));
      break;
    }
  }

  if (markup.kind != TextMarkupKind::squiggly) {
    out << "f\n";
  }
  return std::move(out).str();
}

/// Catmull-Rom through `points`, emitted as the cubic beziers PDF has. The
/// tangent at each point is half the vector between its neighbours.
void smooth_path(std::ostringstream &out, const InkStroke &stroke) {
  const std::size_t count = stroke.size() / 2;
  const auto x = [&](const std::size_t i) {
    return stroke[2 * std::clamp<std::size_t>(i, 0, count - 1)];
  };
  const auto y = [&](const std::size_t i) {
    return stroke[2 * std::clamp<std::size_t>(i, 0, count - 1) + 1];
  };

  out << number(x(0)) << ' ' << number(y(0)) << " m\n";
  if (count == 1) {
    // a dot: a zero-length segment, which round caps render as a disc
    out << number(x(0)) << ' ' << number(y(0)) << " l\n";
    return;
  }
  for (std::size_t i = 0; i + 1 < count; ++i) {
    const double c1x = x(i) + (x(i + 1) - x(i == 0 ? 0 : i - 1)) / 6;
    const double c1y = y(i) + (y(i + 1) - y(i == 0 ? 0 : i - 1)) / 6;
    const double c2x = x(i + 1) - (x(i + 2) - x(i)) / 6;
    const double c2y = y(i + 1) - (y(i + 2) - y(i)) / 6;
    out << number(c1x) << ' ' << number(c1y) << ' ' << number(c2x) << ' '
        << number(c2y) << ' ' << number(x(i + 1)) << ' ' << number(y(i + 1))
        << " c\n";
  }
}

std::string ink_appearance(const Ink &ink) {
  std::ostringstream out;
  out << "/G0 gs\n";
  const auto &[r, g, b] = ink.common.color;
  out << number(r) << ' ' << number(g) << ' ' << number(b) << " RG\n";
  out << number(ink.width) << " w 1 J 1 j\n";
  for (const InkStroke &stroke : ink.strokes) {
    smooth_path(out, stroke);
    out << "S\n";
  }
  return std::move(out).str();
}

} // namespace

ObjectReference write_text_markup(IncrementalWriter &writer,
                                  const TextMarkup &markup) {
  if (markup.quads.empty()) {
    throw std::invalid_argument("text markup has no quads");
  }

  const Box box = box_of(markup.quads);
  const ObjectReference appearance = writer.mint_object();
  const ObjectReference annotation = writer.mint_object();

  // 11.6.4.1: only a highlight is a wash over the text; the others are marks
  // drawn on top and blend normally.
  const bool multiply = markup.kind == TextMarkupKind::highlight;
  writer.set_stream_object(
      appearance,
      appearance_dictionary(
          box,
          graphics_state_resources(markup.common.opacity,
                                   multiply ? "Multiply" : ""),
          multiply),
      text_markup_appearance(markup));

  Dictionary dictionary;
  dictionary["Type"] = Object(Name{"Annot"});
  dictionary["Subtype"] = Object(Name{std::string(subtype_of(markup.kind))});
  dictionary["Rect"] = rectangle(box);
  Array quad_points;
  for (const Quad &quad : markup.quads) {
    for (const double v : quad) {
      quad_points.holder().emplace_back(Real{v});
    }
  }
  dictionary["QuadPoints"] = Object(std::move(quad_points));
  write_common(dictionary, markup.common, annotation);
  Dictionary appearances;
  appearances["N"] = Object(appearance);
  dictionary["AP"] = Object(std::move(appearances));

  writer.set_object(annotation, Object(std::move(dictionary)));
  return annotation;
}

ObjectReference write_ink(IncrementalWriter &writer, const Ink &ink) {
  if (ink.strokes.empty()) {
    throw std::invalid_argument("ink has no strokes");
  }
  for (const InkStroke &stroke : ink.strokes) {
    if (stroke.empty() || stroke.size() % 2 != 0) {
      throw std::invalid_argument("ink stroke is not a sequence of x y pairs");
    }
  }

  // the stroke straddles the path, and a round join can reach half a width out
  const Box box = box_of(ink.strokes).grown(ink.width);
  const ObjectReference appearance = writer.mint_object();
  const ObjectReference annotation = writer.mint_object();

  writer.set_stream_object(
      appearance,
      appearance_dictionary(
          box, graphics_state_resources(ink.common.opacity, ""), false),
      ink_appearance(ink));

  Dictionary dictionary;
  dictionary["Type"] = Object(Name{"Annot"});
  dictionary["Subtype"] = Object(Name{"Ink"});
  dictionary["Rect"] = rectangle(box);
  Array ink_list;
  for (const InkStroke &stroke : ink.strokes) {
    Array points;
    for (const double v : stroke) {
      points.holder().emplace_back(Real{v});
    }
    ink_list.holder().emplace_back(std::move(points));
  }
  dictionary["InkList"] = Object(std::move(ink_list));
  Dictionary border;
  border["W"] = Object(Real{ink.width});
  dictionary["BS"] = Object(std::move(border));
  write_common(dictionary, ink.common, annotation);
  Dictionary appearances;
  appearances["N"] = Object(appearance);
  dictionary["AP"] = Object(std::move(appearances));

  writer.set_object(annotation, Object(std::move(dictionary)));
  return annotation;
}

void append_page_annotations(IncrementalWriter &writer, const Page &page,
                             const std::vector<ObjectReference> &annotations) {
  if (annotations.empty()) {
    return;
  }

  Dictionary dictionary = page.object.as_dictionary();
  const Object &existing = dictionary.get("Annots");

  const auto extend = [&annotations](Array array) {
    for (const ObjectReference &annotation : annotations) {
      array.holder().emplace_back(annotation);
    }
    return array;
  };

  // `/Annots` may be an indirect array shared with nothing else; rewriting it
  // in place leaves the page dictionary untouched.
  if (existing.is_reference()) {
    const ObjectReference reference = existing.as_reference();
    const Object &array = writer.parser().read_object(reference).object;
    writer.set_object(
        reference,
        Object(extend(array.is_array() ? array.as_array() : Array{})));
    return;
  }

  dictionary["Annots"] =
      Object(extend(existing.is_array() ? existing.as_array() : Array{}));
  writer.set_object(page.object_reference, Object(std::move(dictionary)));
}

} // namespace odr::internal::pdf
