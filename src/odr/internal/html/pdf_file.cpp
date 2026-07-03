#include <odr/internal/html/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/cff_transform.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/font/sfnt_transform.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/pdf/pdf_page_extractor.hpp>
#include <odr/internal/util/string_util.hpp>

#include <utf8cpp/utf8/unchecked.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odr::internal::html {

namespace {

/// Round to 0.01 user-space units; sub-precision beyond that is invisible and
/// the extra digits add up across a page full of path data.
double round2(const double v) { return std::round(v * 100.0) / 100.0; }

constexpr double pt_to_in = 1.0 / 72.0;

/// Serialize a transform as an SVG `matrix(...)`. Only the translation (e, f)
/// is rounded — it lives in page-box units where 1/100 px is plenty; the linear
/// part (a..d) keeps full precision so small scale/skew factors aren't
/// quantized to zero. Used for `transform`, `gradientTransform` and
/// `patternTransform`.
std::string svg_matrix(const util::math::Transform2D &m) {
  std::ostringstream f;
  f << "matrix(" << m.a << ',' << m.b << ',' << m.c << ',' << m.d << ','
    << round2(m.e) << ',' << round2(m.f) << ')';
  return std::move(f).str();
}

/// Clamp a colour component in [0, 1] to an 8-bit channel value.
std::int32_t to255(const double v) {
  return static_cast<std::int32_t>(
      std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}

/// Convert a PDF device color to a CSS `rgb(...)` string. Non-device color
/// spaces (Separation/ICCBased/… — stage 4.4) and the unknown space fall back
/// to black, the PDF initial color.
std::string device_color_to_css(const pdf::GraphicsState::Color &color) {
  std::int32_t r = 0;
  std::int32_t g = 0;
  std::int32_t b = 0;
  switch (color.space) {
  case pdf::ColorSpace::device_grey:
    r = g = b = to255(color.grey);
    break;
  case pdf::ColorSpace::device_rgb:
    r = to255(color.rgb[0]);
    g = to255(color.rgb[1]);
    b = to255(color.rgb[2]);
    break;
  case pdf::ColorSpace::device_cmyk: {
    // Naive CMYK -> RGB (no ICC); refined in stage 4.4.
    const double c = color.cmyk[0];
    const double m = color.cmyk[1];
    const double y = color.cmyk[2];
    const double k = color.cmyk[3];
    r = to255((1 - c) * (1 - k));
    g = to255((1 - m) * (1 - k));
    b = to255((1 - y) * (1 - k));
    break;
  }
  case pdf::ColorSpace::unknown:
    break;
  }
  std::ostringstream s;
  s << "rgb(" << r << ',' << g << ',' << b << ')';
  return std::move(s).str();
}

/// Convert an sRGB triple in [0, 1] (a shading colour stop) to a CSS
/// `rgb(...)`.
std::string rgb_to_css(const std::array<double, 3> &rgb) {
  std::ostringstream s;
  s << "rgb(" << to255(rgb[0]) << ',' << to255(rgb[1]) << ',' << to255(rgb[2])
    << ')';
  return std::move(s).str();
}

/// Build an SVG `d` attribute from a path's subpaths, each point mapped through
/// `to_box` (PDF user space -> the page box, y-down). Lines become `L`, cubic
/// Béziers `C`, and an explicitly closed subpath ends with `Z`.
std::string svg_path_d(const std::vector<pdf::Subpath> &subpaths,
                       const util::math::Transform2D &to_box) {
  std::ostringstream d;
  const auto point = [&](const std::array<double, 2> &p) {
    const std::array<double, 2> q = to_box.apply(p[0], p[1]);
    d << ' ' << round2(q[0]) << ' ' << round2(q[1]);
  };
  bool first = true;
  for (const pdf::Subpath &sub : subpaths) {
    d << (first ? "M" : " M");
    first = false;
    point(sub.start);
    for (const pdf::PathSegment &seg : sub.segments) {
      if (seg.kind == pdf::PathSegment::Kind::line) {
        d << " L";
        point(seg.end);
      } else {
        d << " C";
        point(seg.c1);
        point(seg.c2);
        point(seg.end);
      }
    }
    if (sub.closed) {
      d << " Z";
    }
  }
  return std::move(d).str();
}

/// Serialize a painted path to an SVG `<path .../>` fragment in the page
/// viewBox, or "" when it paints nothing. Fill honours the even-odd rule;
/// stroke carries width (CTM-scaled in user space), caps, joins, miter limit
/// and the dash pattern. A zero stroke width renders as a thin hairline.
/// `clip_id`, when non-empty, references a `<clipPath>` installed via
/// `clip-path`. `fill_url_id`, when non-empty, fills the path with that paint
/// server (a shading gradient or a tiling `<pattern>`) instead of `fill_color`.
std::string svg_path_fragment(const pdf::PathElement &path,
                              const util::math::Transform2D &to_box,
                              const std::string &clip_id,
                              const std::string &fill_url_id) {
  if ((!path.fill && !path.stroke) || path.subpaths.empty()) {
    return {};
  }
  std::ostringstream f;
  f << "<path d=\"" << svg_path_d(path.subpaths, to_box) << '"';
  if (!clip_id.empty()) {
    f << " clip-path=\"url(#" << clip_id << ")\"";
  }

  if (path.fill) {
    if (!fill_url_id.empty()) {
      f << " fill=\"url(#" << fill_url_id << ")\"";
    } else {
      f << " fill=\"" << device_color_to_css(path.fill_color) << '"';
    }
    if (path.even_odd) {
      f << " fill-rule=\"evenodd\"";
    }
  } else {
    f << " fill=\"none\"";
  }

  if (path.stroke) {
    f << " stroke=\"" << device_color_to_css(path.stroke_color) << '"';
    // A 0 width is "device-thinnest" in PDF; SVG would draw nothing, so floor
    // it to a sub-point hairline.
    const double width = path.line_width > 0 ? path.line_width : 0.5;
    f << " stroke-width=\"" << round2(width) << '"';
    if (path.line_cap == 1) {
      f << " stroke-linecap=\"round\"";
    } else if (path.line_cap == 2) {
      f << " stroke-linecap=\"square\"";
    }
    if (path.line_join == 1) {
      f << " stroke-linejoin=\"round\"";
    } else if (path.line_join == 2) {
      f << " stroke-linejoin=\"bevel\"";
    } else {
      // miter join: SVG defaults the limit to 4, PDF to 10 — state it.
      f << " stroke-miterlimit=\"" << round2(path.miter_limit) << '"';
    }
    const bool dashed = std::ranges::any_of(
        path.dash_array, [](const double v) { return v > 0; });
    if (dashed) {
      f << " stroke-dasharray=\"";
      for (std::size_t i = 0; i < path.dash_array.size(); ++i) {
        f << (i == 0 ? "" : ",") << round2(path.dash_array[i]);
      }
      f << '"';
      if (path.dash_phase != 0) {
        f << " stroke-dashoffset=\"" << round2(path.dash_phase) << '"';
      }
    }
  }

  f << "/>";
  return std::move(f).str();
}

/// Serialize an image XObject to an SVG `<image>` fragment in the page viewBox,
/// or "" when it carries no pass-through bytes. The image fills the unit square
/// in user space (ISO 32000-1 8.10.5); the transform maps that square — through
/// a vertical flip (the image's first row is its top, SVG draws y-down) and the
/// CTM — into the page box. `clip_id`, when non-empty, installs a clip via a
/// wrapping `<g clip-path>`. The clip geometry is in the page viewBox
/// (`userSpaceOnUse`), but the `<image>` carries its own `transform`, so a
/// `clip-path` placed *on the image* would be resolved in the image's
/// post-transform unit-square space and clip the whole image away. The `<g>`
/// carries no transform, so the clip is read in the viewBox where it lives.
std::string svg_image_fragment(const pdf::ImageElement &image,
                               const util::math::Transform2D &to_box,
                               const std::string &clip_id) {
  if (image.data.empty()) {
    return {};
  }
  constexpr util::math::Transform2D flip =
      util::math::Transform2D::scaling_translation(1, -1, 0, 1);
  const util::math::Transform2D m = flip * image.transform * to_box;

  std::ostringstream f;
  if (!clip_id.empty()) {
    f << "<g clip-path=\"url(#" << clip_id << ")\">";
  }
  f << R"(<image width="1" height="1" preserveAspectRatio="none" transform=")"
    << svg_matrix(m) << '"';
  f << " href=\"" << file_to_url(image.data, image.mime) << "\"/>";
  if (!clip_id.empty()) {
    f << "</g>";
  }
  return std::move(f).str();
}

/// Shared bookkeeping for the per-page `<defs>` registries below (clips,
/// gradients, tiling patterns): a signature->id cache that deduplicates
/// repeated definitions, a per-page monotonic id counter, and the accumulated
/// `<defs>` markup (emitted once into the page's hidden `<svg>`). Ids are
/// namespaced per page as `<prefix><page>_<n>`.
class DefsRegistry {
public:
  explicit DefsRegistry(const std::uint32_t page) : m_page{page} {}

  [[nodiscard]] std::string defs() const { return m_defs.str(); }

protected:
  /// The id for `signature`, minting `<prefix><page>_<n>` the first time it is
  /// seen. `inserted` is true only on that first sight — when the caller still
  /// needs to emit the definition into `m_defs`.
  struct Entry {
    std::string id;
    bool inserted;
  };
  Entry intern(const std::string &signature, const char *prefix) {
    const auto [it, inserted] = m_id_by_signature.try_emplace(signature);
    if (inserted) {
      it->second = std::string(prefix) + std::to_string(m_page) + "_" +
                   std::to_string(++m_count);
    }
    return {it->second, inserted};
  }

  std::ostringstream m_defs;

private:
  std::uint32_t m_page;
  std::uint32_t m_count{0};
  std::unordered_map<std::string, std::string> m_id_by_signature;
};

/// Registers a page's clip regions as nested `<clipPath>` defs, deduplicating
/// shared prefixes. PDF's current clip is the *intersection* of an ordered list
/// of regions; SVG expresses intersection by chaining `clip-path` from one
/// `<clipPath>` to the next, so region i's clipPath references region i-1's and
/// the painted element references the last. Ids are namespaced per page
/// (`c<page>_<n>`).
class ClipRegistry : public DefsRegistry {
public:
  using DefsRegistry::DefsRegistry;

  /// The clipPath id to reference on a path painted under `clip`, registering
  /// any not-yet-seen regions. Empty when `clip` is empty (unclipped).
  std::string register_clip(const std::vector<pdf::ClipPath> &clip,
                            const util::math::Transform2D &to_box) {
    std::string signature;
    std::string parent;
    for (const pdf::ClipPath &region : clip) {
      const std::string d = svg_path_d(region.subpaths, to_box);
      signature += region.even_odd ? 'E' : 'N';
      signature += d;
      signature += ';';
      const auto [id, inserted] = intern(signature, "c");
      if (inserted) {
        m_defs << "<clipPath id=\"" << id << '"';
        if (!parent.empty()) {
          m_defs << " clip-path=\"url(#" << parent << ")\"";
        }
        m_defs << "><path d=\"" << d << '"';
        if (region.even_odd) {
          m_defs << " clip-rule=\"evenodd\"";
        }
        m_defs << "/></clipPath>";
      }
      parent = id;
    }
    return parent;
  }
};

/// Registers a page's shadings (axial/radial) as `<linearGradient>`/
/// `<radialGradient>` defs, deduplicating by shading and placement. The
/// shading's pre-sampled colour stops become `<stop>`s; `gradientTransform`
/// (shading space -> page box) places the gradient in the page's user space, so
/// referencing elements use `gradientUnits="userSpaceOnUse"`. Ids are
/// namespaced per page (`g<page>_<n>`).
///
/// DEFERRED (out of scope for this stage): PDF `/Extend` is approximated by
/// SVG's default `pad` spread (the end stops extend outward), so a non-extended
/// shading is over-painted beyond its interval instead of being masked to it;
/// `Shading::background` and `Shading::bbox` are likewise not yet honoured.
/// Honouring them needs the fill clipped to the gradient band/annulus.
class GradientRegistry : public DefsRegistry {
public:
  using DefsRegistry::DefsRegistry;

  /// The gradient id to reference via `fill="url(#id)"` for `shading` placed by
  /// `m` (shading space -> page box). Empty for an unrepresentable shading.
  std::string register_gradient(const pdf::Shading &shading,
                                const util::math::Transform2D &m) {
    if ((shading.type != 2 && shading.type != 3) || shading.stops.empty()) {
      return {};
    }
    std::ostringstream sig;
    sig << shading.type << ':' << static_cast<const void *>(&shading) << ':'
        << m.a << ',' << m.b << ',' << m.c << ',' << m.d << ',' << m.e << ','
        << m.f;
    const auto [id, inserted] = intern(sig.str(), "g");
    if (!inserted) {
      return id;
    }

    const std::array<double, 6> &c = shading.coords;
    if (shading.type == 2) {
      m_defs << "<linearGradient id=\"" << id << "\" x1=\"" << c[0]
             << "\" y1=\"" << c[1] << "\" x2=\"" << c[2] << "\" y2=\"" << c[3]
             << '"';
    } else {
      // Radial: the outer circle (x1,y1,r1) is SVG's (cx,cy,r); the inner
      // circle (x0,y0,r0) is the focal point and radius (fr is SVG2).
      m_defs << "<radialGradient id=\"" << id << "\" cx=\"" << c[3]
             << "\" cy=\"" << c[4] << "\" r=\"" << c[5] << "\" fx=\"" << c[0]
             << "\" fy=\"" << c[1] << "\" fr=\"" << c[2] << '"';
    }
    m_defs << " gradientUnits=\"userSpaceOnUse\" gradientTransform=\""
           << svg_matrix(m) << "\">";
    for (const pdf::GradientStop &stop : shading.stops) {
      m_defs << "<stop offset=\"" << round2(stop.offset) << "\" stop-color=\""
             << rgb_to_css(stop.rgb) << "\"/>";
    }
    m_defs << (shading.type == 2 ? "</linearGradient>" : "</radialGradient>");
    return id;
  }
};

/// Serialize an `sh` shading flood to an SVG `<rect>` covering the page box,
/// filled with `gradient_id` and bounded by `clip_id` (the clip in force at
/// `sh` time). Returns "" when the shading produced no gradient. The rect spans
/// the whole page; the clip (and the gradient's own extent) bound the paint.
std::string svg_shading_fragment(const std::string &gradient_id,
                                 const std::string &clip_id, const double width,
                                 const double height) {
  if (gradient_id.empty()) {
    return {};
  }
  std::ostringstream f;
  f << "<rect x=\"0\" y=\"0\" width=\"" << round2(width) << "\" height=\""
    << round2(height) << "\" fill=\"url(#" << gradient_id << ")\"";
  if (!clip_id.empty()) {
    f << " clip-path=\"url(#" << clip_id << ")\"";
  }
  f << "/>";
  return std::move(f).str();
}

/// Registers a page's tiling patterns (`/PatternType 1`) as SVG `<pattern>`
/// defs. The pattern's content stream is run as a mini page (`extract_page`)
/// into tile fragments laid out in pattern space; the `<pattern>` repeats them
/// every `/XStep`/`/YStep`, and `patternTransform` (pattern space -> page box)
/// places the lattice. An uncoloured pattern (`/PaintType 2`) ignores its
/// content's own colours and paints in the path's fill colour, so the cache key
/// folds that colour in. Each cell is clipped to its `/BBox` so marks outside
/// the cell (or in the gap when a step exceeds the BBox) don't leak into the
/// tile. Ids are namespaced per page (`pat<page>_<n>`). Only paths and images
/// inside the tile are rendered (nested text/shadings/patterns are skipped —
/// rare). Returns "" for an unrepresentable pattern.
class PatternRegistry : public DefsRegistry {
public:
  using DefsRegistry::DefsRegistry;

  std::string register_pattern(const pdf::Pattern &pattern,
                               const util::math::Transform2D &m,
                               const pdf::GraphicsState::Color &fill_color,
                               const Logger &logger) {
    if (pattern.resources == nullptr || pattern.content.empty() ||
        pattern.x_step == 0 || pattern.y_step == 0) {
      return {};
    }
    const bool uncoloured = pattern.paint_type == 2;
    std::ostringstream sig;
    sig << static_cast<const void *>(&pattern) << ':' << m.a << ',' << m.b
        << ',' << m.c << ',' << m.d << ',' << m.e << ',' << m.f;
    if (uncoloured) {
      sig << ':' << device_color_to_css(fill_color);
    }
    const auto [id, inserted] = intern(sig.str(), "pat");
    if (!inserted) {
      return id;
    }

    // Tile content is laid out in pattern space (identity page transform); the
    // y-flip and placement live in `patternTransform`.
    std::ostringstream tile;
    for (const pdf::PageElement &element :
         pdf::extract_page(pattern.content, *pattern.resources, logger)) {
      if (const auto *path = std::get_if<pdf::PathElement>(&element)) {
        pdf::PathElement painted = *path;
        if (uncoloured) {
          painted.fill_color = fill_color;
          painted.stroke_color = fill_color;
        }
        tile << svg_path_fragment(painted, util::math::Transform2D(), "", "");
      } else if (const auto *image = std::get_if<pdf::ImageElement>(&element)) {
        tile << svg_image_fragment(*image, util::math::Transform2D(), "");
      }
    }

    m_defs << "<pattern id=\"" << id
           << "\" patternUnits=\"userSpaceOnUse\" x=\""
           << round2(pattern.bbox[0]) << "\" y=\"" << round2(pattern.bbox[1])
           << "\" width=\"" << round2(std::abs(pattern.x_step))
           << "\" height=\"" << round2(std::abs(pattern.y_step))
           << "\" patternTransform=\"" << svg_matrix(m) << "\">";
    // Clip each cell to its `/BBox` (ISO 32000-1 8.7.3.1). An overlapping
    // lattice (a step smaller than the BBox) can't be expressed as a single SVG
    // `<pattern>` and is not reproduced.
    const double bbox_w = pattern.bbox[2] - pattern.bbox[0];
    const double bbox_h = pattern.bbox[3] - pattern.bbox[1];
    if (bbox_w > 0 && bbox_h > 0) {
      m_defs << "<clipPath id=\"" << id << "c\"><rect x=\""
             << round2(pattern.bbox[0]) << "\" y=\"" << round2(pattern.bbox[1])
             << "\" width=\"" << round2(bbox_w) << "\" height=\""
             << round2(bbox_h) << "\"/></clipPath><g clip-path=\"url(#" << id
             << "c)\">" << std::move(tile).str() << "</g>";
    } else {
      m_defs << std::move(tile).str();
    }
    m_defs << "</pattern>";
    return id;
  }
};

/// Deduplicates CSS declarations into atomic, single-property classes. PDF text
/// emits one absolutely-positioned line block per detected line, and the same
/// font sizes, offsets and spacings recur across the (potentially millions of)
/// elements. Writing each declaration inline bloats the document. Instead,
/// every distinct declaration is registered once here, named `<prefix><n>` in
/// first-seen order (e.g. `f1`, `f2` for font sizes, `t1` for a top offset),
/// emitted once in <head>, and referenced by class on each element. This is
/// representation-only: the computed style of every element is unchanged.
class AtomicStyles {
public:
  /// `prefix` selects the property family; `declaration` is a full CSS
  /// declaration without trailing ';' (e.g. "font-size:9.96pt"). Returns the
  /// class name to add to the element.
  const std::string &intern(const std::string &prefix,
                            std::string declaration) {
    const auto [it, inserted] =
        m_class_by_declaration.try_emplace(std::move(declaration));
    if (inserted) {
      it->second = prefix + std::to_string(++m_count_by_prefix[prefix]);
      m_order.push_back(&*it);
    }
    return it->second;
  }

  /// Writes one rule per line (`.f1{font-size:9.96pt}`) so regeneration diffs
  /// stay legible. Each rule is preceded by a newline; the caller has already
  /// written the constant rules on the opening `<style>` line.
  void write_rules(std::ostream &o) const {
    for (const auto *entry : m_order) {
      o << "\n." << entry->second << '{' << entry->first << '}';
    }
  }

private:
  /// Node-based map: pointers stored in `m_order` stay valid across
  /// insertions.
  std::unordered_map<std::string, std::string> m_class_by_declaration;
  std::unordered_map<std::string, int> m_count_by_prefix;
  std::vector<const std::pair<const std::string, std::string> *> m_order;
};

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(PdfFile pdf_file, HtmlConfig config,
                  std::shared_ptr<Logger> logger)
      : HtmlService(std::move(config), std::move(logger)),
        m_pdf_file{std::move(pdf_file)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "document", 0, "document.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    if (path == "document.html") {
      return true;
    }
    return false;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "document.html") {
      return "text/html";
    }
    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "document.html") {
      HtmlWriter writer(out, config());
      write_document(writer);
      return;
    }
    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == "document.html") {
      return write_document(out);
    }
    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_document(HtmlWriter &out) const {
    if (config().pdf_text_mode == PdfTextMode::single_layer) {
      return write_document_single_layer(out);
    }
    return write_document_dual_layer(out);
  }

  // =========================================================================
  // DUAL-LAYER MODE
  // =========================================================================
  //
  // Two separate layers per page:
  //
  //  Visual layer (`<div class="vis" aria-hidden="true">`): paint-order glyph
  //  rendering. Text runs are grouped into line blocks (`<div class="t ...">`)
  //  by baseline; runs within a block flow inline, each nudged by a
  //  `margin-left`. A path or image in paint order closes the open block and
  //  is emitted into an SVG; the next text opens a fresh block. Fonts are
  //  re-encoded to the PUA (no real-Unicode cmap entries needed — the visual
  //  layer is `user-select:none`). Invisible text (Tr 3/7) is omitted here.
  //
  //  Selection layer (`<div class="sel">`): transparent, selectable real
  //  Unicode. Text runs are grouped into per-line divs (`<div class="t ...
  //  i">`) in content-stream order; space detection inserts separator spans
  //  on line/column breaks or wide gaps. Each run span is
  //  `display:inline-block; width:Xpt` with CSS `text-align:justify;
  //  text-align-last:justify; text-justify:inter-character` so the browser
  //  spreads the characters to fill the PDF advance without JavaScript.
  //  For gap spans between runs a zero-content `display:inline-block;
  //  width:Ypt` span is emitted.

  /// One run inside a visual line block. `classes` carries margin-left, font
  /// size, font-family+colour — the line block holds placement only.
  struct VisRunOut {
    std::string classes;
    std::string text; ///< PUA glyph string (or real unicode for fallback path)
  };
  /// One line block in the visual layer: absolutely positioned at the first
  /// run's origin. Runs flow inline, each nudged by margin-left.
  struct VisLineOut {
    std::string classes; ///< "t lN tN [mN]" (or matrix transform)
    std::vector<VisRunOut> runs;
  };
  struct PathOut {
    std::string svg;
  };
  /// Visual-layer paint-order item: a line block of glyphs, or an SVG
  /// fragment.
  using VisItem = std::variant<VisLineOut, PathOut>;

  /// One run in the selection layer: an inline-block span with a fixed width
  /// (for CSS justify) and optional margin-left (gap from previous run). An
  /// empty `text` with non-zero `width` is a spacer-only span (no text node).
  struct SelRunOut {
    std::string classes; ///< "sr wN [mlN]"
    std::string text; ///< real unicode (HTML-escaped), may be empty for spacer
  };
  /// One line block in the selection layer: absolutely positioned,
  /// transparent.
  struct SelLineOut {
    std::string classes; ///< "t lN tN i"
    std::vector<SelRunOut> runs;
  };

  struct DualPageOut {
    std::string classes;
    double width{0};
    double height{0};
    std::vector<VisItem> vis_items;
    std::vector<SelLineOut> sel_lines;
    std::string clip_defs;
  };

  HtmlResources write_document_dual_layer(HtmlWriter &out) const {
    HtmlResources resources;

    const auto &pdf_file =
        dynamic_cast<const pdf::PdfFile &>(*m_pdf_file.impl());
    pdf::DocumentParser parser = pdf_file.create_parser(*m_logger);
    const std::unique_ptr<pdf::Document> document = parser.parse_document();
    const std::vector<pdf::Page *> pages = document->collect_pages();

    AtomicStyles styles;
    std::vector<DualPageOut> pages_out;
    pages_out.reserve(pages.size());

    // Font management — visual layer only needs PUA glyphs.
    std::uint32_t family_count = 0;
    std::string font_faces;
    std::string font_styles; // per-font `.fvN` (visible) / `.fnN` (invisible)
    std::vector<const pdf::Font *> accepted_fonts;
    // Which classes are used: [0]=fv (visible), [1]=fn (invisible).
    std::vector<std::array<bool, 2>> font_class_used;
    std::unordered_map<const pdf::Font *, std::uint32_t> family_index;

    const auto font_family = [&](const pdf::Font *font) {
      return intern_font(family_index, family_count, font, [&](std::uint32_t) {
        accepted_fonts.push_back(font);
        font_class_used.push_back({false, false});
      });
    };

    const auto add_class = [&styles](std::string &classes,
                                     const std::string &prefix,
                                     std::string declaration) {
      classes += ' ';
      classes += styles.intern(prefix, std::move(declaration));
    };

    // Strips a trailing `wN` (width) class token, if present, so a merged
    // selection run's width can be re-declared. Assumes at most one `w`
    // class is ever attached to a selection run.
    const auto strip_width_class = [](std::string &classes) {
      const std::size_t pos = classes.rfind(' ');
      if (pos == std::string::npos) {
        return;
      }
      const std::string_view tail(classes.data() + pos + 1,
                                  classes.size() - pos - 1);
      if (tail.size() > 1 && tail.front() == 'w' &&
          std::all_of(tail.begin() + 1, tail.end(),
                      [](unsigned char c) { return std::isdigit(c) != 0; })) {
        classes.resize(pos);
      }
    };

    for (pdf::Page *page : pages) {
      const PageBox pb = begin_page(*page, add_class);
      const double width = pb.width;
      const double height = pb.height;
      const util::math::Transform2D &to_box = pb.to_box;

      DualPageOut &page_out = pages_out.emplace_back();
      page_out.classes = pb.classes;
      page_out.width = width;
      page_out.height = height;

      std::string stream;
      for (const auto &ref : page->contents_reference) {
        stream += parser.read_decoded_stream(ref);
        stream += '\n';
      }

      ClipRegistry clips(static_cast<std::uint32_t>(pages_out.size()));
      GradientRegistry gradients(static_cast<std::uint32_t>(pages_out.size()));
      PatternRegistry patterns(static_cast<std::uint32_t>(pages_out.size()));

      // Visual layer state: open line block.
      /// index of open VisLineOut in vis_items, -1 = none
      std::int32_t vis_cur_line = -1;
      double vis_prev_end = 0;
      double vis_prev_baseline = 0;
      double vis_prev_font_pt = 0;
      bool vis_prev_was_matrix = false;
      const auto vis_close_line = [&] { vis_cur_line = -1; };

      // Selection layer state: in-content-stream reading-order grouping.
      bool sel_have_prev = false;
      double sel_prev_baseline = 0;
      double sel_prev_end = 0;
      /// previous run's advance height, for the line-break test (see
      /// starts_new_line)
      double sel_prev_font_pt = 0;
      bool sel_prev_ends_space = false;
      bool sel_prev_was_matrix = false;
      std::int32_t sel_cur_line = -1;
      /// `ox` of the open `.sr` run's start, for recomputing its width on
      /// merge.
      double sel_cur_run_start_ox = 0;
      /// font-size of the previous element, for the trailing space that closes
      /// its line.
      double sel_prev_font_size_pt = 0;

      for (const pdf::PageElement &element :
           pdf::extract_page(stream, *page->resources, *m_logger)) {
        if (handle_graphic_element(
                element, to_box, width, height, clips, gradients, patterns,
                *m_logger, [&] { vis_close_line(); },
                [&](std::string frag) {
                  page_out.vis_items.push_back(PathOut{std::move(frag)});
                })) {
          continue;
        }

        const pdf::TextElement &text = std::get<pdf::TextElement>(element);
        // TODO(clip text): clip not applied to text; see STAGE4_PLAN.md.
        const std::uint32_t font =
            text.font != nullptr ? font_family(text.font) : 0;
        if (text.text.empty() && font == 0) {
          continue;
        }

        const auto [m, invisible, is_matrix, asc, scale, ox, baseline, extent,
                    font_pt, font_size_pt] = run_geometry(text, to_box);
        const std::string color_suffix = color_class(text, invisible, styles);

        // --- Visual layer ---------------------------------------------------
        // Invisible runs (Tr 3/7) paint nothing; omit them from the visual
        // layer. They contribute only to the selection layer.
        if (!invisible) {
          // Determine if this run continues the current visual line block.
          bool new_vis_line =
              is_matrix || vis_prev_was_matrix || vis_cur_line < 0;
          double vis_margin_pt = 0;
          if (!new_vis_line && vis_prev_font_pt > 0) {
            if (starts_new_line(baseline, vis_prev_baseline, ox, vis_prev_end,
                                vis_prev_font_pt)) {
              new_vis_line = true;
            } else {
              vis_margin_pt = round2(ox - vis_prev_end);
            }
          }

          if (new_vis_line) {
            // Build the line block's placement classes.
            std::string line_base = "t";
            add_position_classes(line_base, add_class, m, is_matrix, ox,
                                 baseline, asc * text.size);
            VisLineOut line_out;
            line_out.classes = std::move(line_base);
            page_out.vis_items.push_back(std::move(line_out));
            vis_cur_line = static_cast<int>(page_out.vis_items.size()) - 1;
          }

          // Build the run's span classes: font-size, font-family+colour,
          // margin-left (gap from previous run's right edge, or the line
          // block's new-run offset from its own origin for new-line runs).
          std::string run_classes = "g"; // user-select:none
          add_class(run_classes, "f", pt_decl("font-size", font_size_pt));
          if (font != 0) {
            run_classes += ' ';
            run_classes += font_class(font_class_used, font, invisible);
          }
          if (vis_margin_pt != 0) {
            add_class(run_classes, "ml", pt_decl("margin-left", vis_margin_pt));
          }
          run_classes += color_suffix;

          // Run text: PUA glyphs for embedded font, or real unicode for
          // fallback.
          std::string run_text;
          if (font != 0) {
            run_text = escape_text(glyph_run_str(*text.font, text.codes));
          } else {
            run_text = escape_text(text.text);
          }

          if (const double cs_pt = round2(text.char_spacing * scale);
              cs_pt != 0) {
            add_class(run_classes, "s", pt_decl("letter-spacing", cs_pt));
          }
          // CSS `word-spacing` only affects real U+0020 separators, so it is
          // inert on the PUA glyph runs (font != 0, which never emit a literal
          // space) — apply it solely on the real-unicode fallback path. Skip
          // composite fonts: PDF Tw applies only to single-byte code 32, as in
          // the single-layer path.
          if (font == 0 && !(text.font != nullptr && text.font->composite)) {
            if (const double ws_pt = round2(text.word_spacing * scale);
                ws_pt != 0) {
              add_class(run_classes, "ws", pt_decl("word-spacing", ws_pt));
            }
          }

          std::get<VisLineOut>(page_out.vis_items[vis_cur_line])
              .runs.push_back(
                  VisRunOut{std::move(run_classes), std::move(run_text)});

          vis_prev_end = ox + extent;
          vis_prev_baseline = baseline;
          vis_prev_font_pt = font_pt;
          vis_prev_was_matrix = is_matrix;
        }

        // --- Selection layer -----------------------------------------------
        // Transparent, selectable real-unicode text in content-stream
        // (reading) order. Each run is an inline-block span with a fixed width
        // (the PDF advance) and CSS `text-justify:inter-character` so the
        // browser distributes characters to fill that width without JS. Gaps
        // between runs on the same baseline are zero-content spacer spans.
        // Matrix runs get their own single-run line block.
        if (!text.text.empty()) {
          const double width_pt = round2(extent);
          const double gap_pt = std::max(0.0, ox - sel_prev_end);
          const bool starts_space = text.text.front() == ' ';
          // `core` is the run text with a leading inferred space stripped
          // (the gap between runs is covered by the spacer span, not the
          // run text).
          std::string core = starts_space ? text.text.substr(1) : text.text;

          bool new_sel_line =
              !sel_have_prev || is_matrix || sel_prev_was_matrix;
          bool sel_gap = false;
          if (sel_have_prev && sel_prev_font_pt > 0 && !new_sel_line) {
            new_sel_line = starts_new_line(baseline, sel_prev_baseline, ox,
                                           sel_prev_end, sel_prev_font_pt);
            sel_gap = ox - sel_prev_end > 0.25 * sel_prev_font_pt;
          }

          if (new_sel_line) {
            // Close previous line with a trailing space if needed. `sg`
            // (spacer), not `sr` (text run) — it carries no PDF-derived
            // width, just a lone space.
            if (sel_cur_line >= 0 && sel_have_prev && !sel_prev_ends_space) {
              std::string space_cls = "sg";
              add_class(space_cls, "f",
                        pt_decl("font-size", sel_prev_font_size_pt));
              page_out.sel_lines[sel_cur_line].runs.push_back(
                  SelRunOut{std::move(space_cls), " "});
            }
            // Build the line block's placement.
            std::string sel_base = "t";
            add_position_classes(sel_base, add_class, m, is_matrix, ox,
                                 baseline, asc * text.size);
            sel_base += " i"; // transparent
            page_out.sel_lines.push_back(SelLineOut{std::move(sel_base), {}});
            sel_cur_line = static_cast<int>(page_out.sel_lines.size()) - 1;
            // Emit the run span.
            if (!core.empty()) {
              std::string cls = "sr";
              add_class(cls, "f", pt_decl("font-size", font_size_pt));
              if (width_pt > 0 && !is_matrix) {
                add_class(cls, "w", pt_decl("width", width_pt));
              }
              page_out.sel_lines[sel_cur_line].runs.push_back(
                  SelRunOut{std::move(cls), escape_markup(std::move(core))});
              sel_cur_run_start_ox = ox;
            }
          } else if (sel_gap || sel_prev_ends_space || starts_space) {
            // Emit a spacer span for the gap, then the run span.
            std::vector<SelRunOut> &runs =
                page_out.sel_lines[sel_cur_line].runs;
            if (!sel_prev_ends_space && !runs.empty()) {
              // Spacer: display:inline-block with the gap width.
              std::string gap_cls = "sg";
              add_class(gap_cls, "f", pt_decl("font-size", font_size_pt));
              const double rounded_gap = round2(gap_pt);
              if (rounded_gap > 0) {
                add_class(gap_cls, "w", pt_decl("width", rounded_gap));
              }
              runs.push_back(SelRunOut{std::move(gap_cls), " "});
            }
            if (!core.empty()) {
              std::string cls = "sr";
              add_class(cls, "f", pt_decl("font-size", font_size_pt));
              if (width_pt > 0 && !is_matrix) {
                add_class(cls, "w", pt_decl("width", width_pt));
              }
              runs.push_back(
                  SelRunOut{std::move(cls), escape_markup(std::move(core))});
              sel_cur_run_start_ox = ox;
            }
          } else {
            // Tight continuation on the same baseline — merge into the
            // previous selection run's text so the browser treats the whole
            // sequence as one word for double-click and find-in-page. Widen
            // the run's declared width to the full merged extent (measured
            // from where that run started) so CSS justify keeps spreading
            // characters across the true PDF advance instead of the first
            // sub-run's narrower one.
            std::vector<SelRunOut> &runs =
                page_out.sel_lines[sel_cur_line].runs;
            if (!runs.empty()) {
              runs.back().text += escape_markup(text.text);
              if (!is_matrix) {
                strip_width_class(runs.back().classes);
                const double merged_width_pt =
                    round2(ox + extent - sel_cur_run_start_ox);
                if (merged_width_pt > 0) {
                  add_class(runs.back().classes, "w",
                            pt_decl("width", merged_width_pt));
                }
              }
            }
          }

          sel_prev_baseline = baseline;
          sel_prev_end = ox + extent;
          sel_prev_font_pt = font_pt;
          sel_prev_ends_space = !text.text.empty() && text.text.back() == ' ';
          sel_prev_was_matrix = is_matrix;
          sel_prev_font_size_pt = font_size_pt;
          sel_have_prev = true;
        }
      }

      // Selection lines are kept in content-stream order. Re-sorting by
      // baseline y would order out-of-order single-column content correctly but
      // interleave multi-column layouts (the stream keeps columns contiguous);
      // reading order can't be recovered by a scalar sort. Proper page
      // segmentation (column detection / XY-cut) is the eventual fix.
      page_out.clip_defs = clips.defs() + gradients.defs() + patterns.defs();
    }

    // Post-pass: re-encode accepted fonts PUA-only.
    for (std::uint32_t i = 0; i < family_count; ++i) {
      write_font_face(*accepted_fonts[i], i, {}, font_class_used[i], font_faces,
                      font_styles);
    }

    // Write HTML.
    write_header_common(out, font_faces, font_styles, styles, [&] {
      // Visual layer glyph spans: not selectable (selection rides the `.sel`
      // layer).
      out.out() << ".g{user-select:none}";
      // Fallback font for the selection layer: `size-adjust` shrinks a local
      // system font so its natural width lands near the PDF-derived `.sr`/`.sg`
      // widths, leaving a small gap for `text-justify:inter-character` to fill.
      // CSS justify only ever *adds* spacing, so undershooting is free (the
      // text just spreads further; the layer is invisible) while overshooting
      // overflows and is clipped by `overflow:hidden` — hence
      // `pdf_dual_layer_fallback_font_size_adjust` (config, 0-1) is
      // deliberately low. The vertical rescale is harmless: `.sr`/`.sg` fix
      // their box height via `line-height:1` and baseline-align via
      // `overflow:hidden` (below), never consulting the font's ascent/descent.
      // If no `pdf_dual_layer_fallback_fonts` resolve locally the rule is
      // skipped and
      // `.i` falls through to plain `sans-serif`.
      if (const std::vector<std::string> &fonts =
              config().pdf_dual_layer_fallback_fonts;
          !fonts.empty()) {
        std::ostringstream ff;
        ff << "@font-face{font-family:sf;src:";
        for (std::size_t i = 0; i < fonts.size(); ++i) {
          if (i != 0) {
            ff << ',';
          }
          ff << "local(\"";
          for (const char c : fonts[i]) {
            if (c == '"' || c == '\\') {
              ff << '\\';
            }
            ff << c;
          }
          ff << "\")";
        }
        const double adjust_pct =
            round2(std::clamp(config().pdf_dual_layer_fallback_font_size_adjust,
                              0.0, 1.0) *
                   100.0);
        ff << ";size-adjust:" << adjust_pct << "%}";
        out.out() << std::move(ff).str();
      }
      // Transparent text for the selection layer line blocks.
      out.out() << ".i{color:transparent;font-family:sf,sans-serif}";
      // Selection-layer run span: inline-block with CSS justify so the browser
      // spreads characters to fill the declared width (the PDF advance) without
      // JS; `overflow:hidden` clips a wider system font. `.t`'s inherited `pre`
      // (not `nowrap`) blocks wrapping while preserving a run's own
      // leading/trailing space, which is real PDF content.
      out.out() << ".sr{display:inline-block;text-align:justify;"
                   "text-align-last:justify;text-justify:inter-character;"
                   "overflow:hidden}";
      // Selection-layer gap spacer: inline-block, no text, just width.
      // `overflow:hidden` matches `.sr`: an inline-block baseline-aligns to its
      // bottom margin edge only when overflow isn't visible, so without it the
      // spacer and run boxes align differently and the spacer shifts in y.
      out.out() << ".sg{display:inline-block;overflow:hidden}";
    });

    const auto write_vis_line = [&](const VisLineOut &line) {
      out.write_element_begin(
          "div", HtmlElementOptions().set_inline(true).set_class(line.classes));
      for (const VisRunOut &run : line.runs) {
        out.write_element_begin(
            "span",
            HtmlElementOptions().set_inline(true).set_class(run.classes));
        out.write_raw(run.text);
        out.write_element_end("span");
      }
      out.write_element_end("div");
    };

    const auto write_sel_line = [&](const SelLineOut &line) {
      out.write_element_begin(
          "div", HtmlElementOptions().set_inline(true).set_class(line.classes));
      for (const SelRunOut &run : line.runs) {
        out.write_element_begin(
            "span",
            HtmlElementOptions().set_inline(true).set_class(run.classes));
        if (!run.text.empty()) {
          out.write_raw(run.text);
        }
        out.write_element_end("span");
      }
      out.write_element_end("div");
    };

    out.write_body_begin();
    for (const DualPageOut &page : pages_out) {
      out.write_element_begin("div",
                              HtmlElementOptions().set_class(page.classes));

      // Visual layer: paint-order graphics and unselectable glyphs.
      out.write_element_begin("div",
                              HtmlElementOptions().set_class("vis").set_extra(
                                  R"(aria-hidden="true")"));
      write_page_items(out, page.clip_defs, page.vis_items, page.width,
                       page.height, write_vis_line);
      out.write_element_end("div"); // .vis

      // Selection layer: transparent, selectable Unicode in reading order.
      out.write_element_begin("div", HtmlElementOptions().set_class("sel"));
      for (const SelLineOut &line : page.sel_lines) {
        write_sel_line(line);
      }
      out.write_element_end("div"); // .sel

      out.write_element_end("div"); // .p
    }
    out.write_body_end();
    out.write_end();

    return resources;
  }

  // =========================================================================
  // SINGLE-LAYER MODE
  // =========================================================================
  //
  // One combined text layer per page (paint order). Text runs are grouped into
  // line blocks (`<div class="t ...">`) in paint order; individual runs flow
  // inline within the block, each nudged by a `margin-left` (the gen-time gap
  // from the previous run's right edge, matching the embedded font's advance
  // exactly when the font's `hmtx` matches the PDF `/Widths`).
  //
  // Unicode mapping uses frequency analysis: a pre-pass over all pages counts
  // (uchar, glyph) co-occurrences per font, then the post-pass picks the
  // most-frequent glyph for each uchar as the cmap entry. This ensures the
  // common case wins instead of an arbitrary first-come-first-serve order.
  //
  // Clean runs (all (uchar, glyph) pairs match the frequency winner) render
  // the real Unicode directly in the embedded font — natively findable and
  // selectable. Unclean visible runs paint their glyphs via
  // `::before{content:attr(data-g)}` CSS generated content (kept out of the
  // DOM text stream so they never break find mid-word), with a zero-width
  // `display:inline-block; overflow:hidden` overlay carrying the real Unicode
  // alongside. No-unicode runs show only the glyph. Invisible (Tr 3/7) and
  // fallback (no embedded font) runs render the real Unicode as ordinary text.

  struct SingleRunOut {
    std::string margin;     ///< "" or a `margin-left` class
    std::string color;      ///< "" or a colour class name (no leading space)
    std::string text;       ///< real Unicode (HTML-escaped), may be empty
    std::string glyph_data; ///< PUA glyph string (non-empty → unclean)
  };
  struct SingleLineOut {
    std::string classes;    ///< "t lN tN [mN] [fvN|fnN] [iN]..."
    std::string font_class; ///< per-font family+colour class on the block
    std::vector<SingleRunOut> runs;
  };
  struct SinglePathOut {
    std::string svg;
  };
  using SingleItem = std::variant<SingleLineOut, SinglePathOut>;

  struct SinglePageOut {
    std::string classes;
    double width{0};
    double height{0};
    std::vector<SingleItem> items;
    std::string clip_defs;
  };

  HtmlResources write_document_single_layer(HtmlWriter &out) const {
    HtmlResources resources;

    const auto &pdf_file =
        dynamic_cast<const pdf::PdfFile &>(*m_pdf_file.impl());
    pdf::DocumentParser parser = pdf_file.create_parser(*m_logger);
    const std::unique_ptr<pdf::Document> document = parser.parse_document();
    const std::vector<pdf::Page *> pages = document->collect_pages();

    // ---- Font registration ------------------------------------------------
    // A real-Unicode scalar gets a cmap entry only inside the BMP and outside
    // the PUA (`U+E000..U+F8FF`), so glyph-deterministic PUA code points are
    // never shadowed.
    const auto collapsible_unicode = [](const char32_t c) {
      return c <= 0xFFFF && !(c >= 0xE000 && c <= 0xF8FF);
    };

    std::uint32_t family_count = 0;
    std::string font_faces;
    std::string font_styles; // ".fvN{...}" / ".fnN{...}"
    std::vector<pdf::Font *> accepted_fonts;
    // Per-font, per-uchar, per-glyph occurrence count (pre-pass).
    // Indexed by font_index - 1.
    std::vector<std::map<char32_t, std::map<std::uint16_t, std::uint32_t>>>
        glyph_freq;
    // Per-font winning uchar→glyph mapping (derived from glyph_freq).
    std::vector<std::map<char32_t, std::uint16_t>> used_unicode;
    // Which per-font classes are used: [0]=fv (visible), [1]=fn (invisible).
    std::vector<std::array<bool, 2>> font_class_used;
    std::unordered_map<const pdf::Font *, std::uint32_t> family_index;

    const auto font_family = [&](pdf::Font *font) {
      return intern_font(family_index, family_count, font, [&](std::uint32_t) {
        accepted_fonts.push_back(font);
        glyph_freq.emplace_back();
        used_unicode.emplace_back();
        font_class_used.push_back({false, false});
      });
    };

    AtomicStyles styles;
    const auto add_class = [&styles](std::string &classes,
                                     const std::string &prefix,
                                     std::string declaration) {
      classes += ' ';
      classes += styles.intern(prefix, std::move(declaration));
    };

    // Build the page streams once (reused for both the pre-pass and main pass).
    std::vector<std::string> page_streams;
    page_streams.reserve(pages.size());
    for (pdf::Page *page : pages) {
      std::string stream;
      for (const auto &ref : page->contents_reference) {
        stream += parser.read_decoded_stream(ref);
        stream += '\n';
      }
      page_streams.push_back(std::move(stream));
    }

    // ---- Pre-pass: frequency analysis ------------------------------------
    // Count (uchar, glyph) co-occurrences per font over all pages. The main
    // pass then uses the frequency winner for each uchar instead of first-come-
    // first-serve, so the most common glyph shape wins its cmap entry.
    // This re-runs `extract_page` on every page (the main pass parses each a
    // second time) — a deliberate tradeoff: re-parsing the already-decoded
    // stream is cheap next to buffering every page's element list in memory.
    for (std::size_t pi = 0; pi < pages.size(); ++pi) {
      const pdf::Page &page = *pages[pi];
      for (const pdf::PageElement &element :
           pdf::extract_page(page_streams[pi], *page.resources, *m_logger)) {
        const auto *text = std::get_if<pdf::TextElement>(&element);
        if (text == nullptr || text->text.empty() || text->font == nullptr) {
          continue;
        }
        const std::uint32_t font = font_family(text->font);
        if (font == 0) {
          continue;
        }
        // Only collapsible-candidate runs contribute to frequency counts.
        if (util::string::utf8_length(text->text) != text->advances.size()) {
          continue;
        }
        auto cp = text->text.begin();
        for (const std::uint32_t code : text->font->codes(text->codes)) {
          const char32_t uchar = utf8::unchecked::next(cp);
          if (!collapsible_unicode(uchar)) {
            continue;
          }
          const std::uint16_t glyph = text->font->glyph_for_code(code);
          ++glyph_freq[font - 1][uchar][glyph];
        }
      }
    }

    // Compute the frequency winner for each (font, uchar): the glyph with the
    // highest count becomes the cmap entry. Ties broken by lower glyph id.
    for (std::uint32_t fi = 0; fi < family_count; ++fi) {
      for (const auto &[uchar, counts] : glyph_freq[fi]) {
        std::uint16_t best_glyph = 0;
        std::uint32_t best_count = 0;
        for (const auto &[glyph, count] : counts) {
          if (count > best_count ||
              (count == best_count && glyph < best_glyph)) {
            best_glyph = glyph;
            best_count = count;
          }
        }
        used_unicode[fi][uchar] = best_glyph;
      }
    }

    // ---- Main pass (pass 1): build page structures -----------------------
    std::vector<SinglePageOut> pages_out;
    pages_out.reserve(pages.size());

    for (std::size_t pi = 0; pi < pages.size(); ++pi) {
      const pdf::Page &page = *pages[pi];
      const PageBox pb = begin_page(page, add_class);
      const double width = pb.width;
      const double height = pb.height;
      const util::math::Transform2D &to_box = pb.to_box;

      SinglePageOut &page_out = pages_out.emplace_back();
      page_out.classes = pb.classes;
      page_out.width = width;
      page_out.height = height;

      ClipRegistry clips(static_cast<std::uint32_t>(pages_out.size()));
      GradientRegistry gradients(static_cast<std::uint32_t>(pages_out.size()));
      PatternRegistry patterns(static_cast<std::uint32_t>(pages_out.size()));

      std::int32_t cur_line = -1;
      std::string cur_flow_key;
      bool prev_was_matrix = false;
      double prev_end = 0;
      double prev_baseline = 0;
      double prev_font_pt = 0;
      const auto close_line = [&] { cur_line = -1; };

      for (const pdf::PageElement &element :
           pdf::extract_page(page_streams[pi], *page.resources, *m_logger)) {
        if (handle_graphic_element(
                element, to_box, width, height, clips, gradients, patterns,
                *m_logger, [&] { close_line(); },
                [&](std::string frag) {
                  page_out.items.push_back(SinglePathOut{std::move(frag)});
                })) {
          continue;
        }

        const pdf::TextElement &text = std::get<pdf::TextElement>(element);
        // TODO(clip text): clip not applied to text; see STAGE4_PLAN.md.
        const std::uint32_t font =
            text.font != nullptr ? font_family(text.font) : 0;
        if (text.text.empty() && font == 0) {
          continue;
        }

        const auto [m, invisible, is_matrix, asc, scale, ox, baseline, extent,
                    font_pt, font_size_pt] = run_geometry(text, to_box);
        const double cs_pt = round2(text.char_spacing * scale);
        const double ws_pt = round2(text.word_spacing * scale);
        const std::string color_suffix = color_class(text, invisible, styles);

        // ---- Run payload ------------------------------------------------
        // Decide whether this is a clean collapse (real unicode renders
        // directly via the frequency-winner cmap entries) or unclean (glyph
        // painted via generated content, unicode as overlay).
        SingleRunOut run;
        // `color_suffix` carries a leading space for the dual-layer paths that
        // concatenate it onto a class string; the single-layer run stores just
        // the class name.
        run.color =
            color_suffix.empty() ? std::string() : color_suffix.substr(1);

        if (font == 0 || invisible) {
          // Fallback / invisible: render real unicode directly.
          run.text = escape_markup(text.text);
        } else {
          // Check collapse: 1:1 text↔codes and every (uchar, glyph) matches
          // the frequency winner.
          // `font != 0` here already implies `text.font != nullptr`.
          bool collapse =
              !text.text.empty() &&
              util::string::utf8_length(text.text) == text.advances.size();
          if (collapse) {
            const std::map<char32_t, std::uint16_t> &won =
                used_unicode[font - 1];
            auto cp = text.text.begin();
            for (const std::uint32_t code : text.font->codes(text.codes)) {
              const char32_t uchar = utf8::unchecked::next(cp);
              const std::uint16_t glyph = text.font->glyph_for_code(code);
              const auto it = won.find(uchar);
              if (!collapsible_unicode(uchar) || it == won.end() ||
                  it->second != glyph) {
                collapse = false;
                break;
              }
            }
          }
          if (collapse) {
            run.text = escape_markup(text.text);
          } else {
            run.glyph_data = glyph_run_str(*text.font, text.codes);
            run.text = escape_markup(text.text); // overlay (empty=no_unicode)
          }
        }
        if (run.text.empty() && run.glyph_data.empty()) {
          continue; // invisible no_unicode
        }

        // ---- Flow grouping -----------------------------------------------
        std::ostringstream fk;
        fk << font << '|' << invisible << '|' << font_size_pt << '|' << cs_pt
           << '|' << ws_pt;
        const std::string flow_key = std::move(fk).str();
        bool new_line = is_matrix || prev_was_matrix || cur_line < 0 ||
                        flow_key != cur_flow_key;
        double margin_pt = 0;
        if (!new_line && prev_font_pt > 0) {
          if (starts_new_line(baseline, prev_baseline, ox, prev_end,
                              prev_font_pt)) {
            new_line = true;
          } else {
            margin_pt = round2(ox - prev_end);
          }
        }

        if (new_line) {
          std::string base = "t";
          add_position_classes(base, add_class, m, is_matrix, ox, baseline,
                               asc * text.size);
          add_class(base, "f", pt_decl("font-size", font_size_pt));
          const bool spacing_one_to_one =
              font != 0 ||
              (text.font != nullptr &&
               util::string::utf8_length(text.text) == text.advances.size());
          if (text.char_spacing != 0 && spacing_one_to_one) {
            add_class(base, "s", pt_decl("letter-spacing", cs_pt));
          }
          if (text.word_spacing != 0 && spacing_one_to_one &&
              !(text.font != nullptr && text.font->composite)) {
            // `ws` = word-spacing everywhere (`w` is width in the dual layer).
            add_class(base, "ws", pt_decl("word-spacing", ws_pt));
          }
          if (font == 0 && invisible) {
            base += " i";
          }

          SingleLineOut line;
          line.classes = std::move(base);
          if (font != 0) {
            line.font_class = font_class(font_class_used, font, invisible);
          }
          line.runs.push_back(std::move(run));
          page_out.items.push_back(std::move(line));
          cur_line = static_cast<int>(page_out.items.size()) - 1;
          cur_flow_key = flow_key;
        } else {
          if (margin_pt != 0) {
            run.margin = styles.intern("ml", pt_decl("margin-left", margin_pt));
          }
          std::get<SingleLineOut>(page_out.items[cur_line])
              .runs.push_back(std::move(run));
        }

        prev_end = ox + extent;
        prev_baseline = baseline;
        prev_font_pt = font_pt;
        prev_was_matrix = is_matrix;
      }

      page_out.clip_defs = clips.defs() + gradients.defs() + patterns.defs();
    }

    // ---- Post-pass: re-encode fonts with frequency-winner cmap entries ---
    for (std::uint32_t i = 0; i < family_count; ++i) {
      write_font_face(*accepted_fonts[i], i, used_unicode[i],
                      font_class_used[i], font_faces, font_styles);
    }

    // ---- Pass 2: write HTML ---------------------------------------------
    write_header_common(out, font_faces, font_styles, styles, [&] {
      // Invisible text render modes (Tr 3/7).
      out.out() << ".i{color:transparent}";
      // Unclean glyphs painted via CSS generated content (`data-g` attr), kept
      // out of the DOM text stream so they never break find/double-click.
      out.out() << ".gl::before{content:attr(data-g)}";
      // Zero-width inline-block overlay carrying the real Unicode of an unclean
      // run: invisible, zero-width, but still found/selected in reading order.
      // `inline-block` lets `width:0` apply (a regular inline box ignores it);
      // `overflow:hidden` clips the invisible text to zero width.
      out.out() << ".ov{display:inline-block;width:0;overflow:hidden;"
                   "color:transparent;vertical-align:baseline}";
    });

    // Helper: derive a run's leading-span class from `head` prefix plus its
    // optional margin-left and colour override.
    const auto run_class = [](const SingleRunOut &run, const char *head) {
      std::string cls = head;
      const auto add = [&](const std::string &t) {
        if (t.empty()) {
          return;
        }
        if (!cls.empty()) {
          cls += ' ';
        }
        cls += t;
      };
      add(run.margin);
      add(run.color);
      return cls;
    };

    const auto write_line = [&](const SingleLineOut &line) {
      std::string classes = line.classes;
      if (!line.font_class.empty()) {
        classes += ' ';
        classes += line.font_class;
      }
      out.write_element_begin(
          "div", HtmlElementOptions().set_inline(true).set_class(classes));
      for (const SingleRunOut &run : line.runs) {
        if (run.glyph_data.empty()) {
          // Clean / invisible / fallback: real Unicode renders directly.
          const std::string cls = run_class(run, "");
          if (cls.empty()) {
            out.write_raw(run.text);
          } else {
            out.write_element_begin(
                "span", HtmlElementOptions().set_inline(true).set_class(cls));
            out.write_raw(run.text);
            out.write_element_end("span");
          }
        } else {
          // Unclean: glyph via generated content, real-unicode overlay.
          out.write_element_begin(
              "span", HtmlElementOptions()
                          .set_inline(true)
                          .set_class(run_class(run, "gl"))
                          .set_attributes(HtmlAttributesVector{
                              {std::string("data-g"), run.glyph_data}}));
          out.write_element_end("span");
          if (!run.text.empty()) {
            out.write_element_begin(
                "span", HtmlElementOptions().set_inline(true).set_class("ov"));
            out.write_raw(run.text);
            out.write_element_end("span");
          }
        }
      }
      out.write_element_end("div");
    };

    out.write_body_begin();
    for (const SinglePageOut &page : pages_out) {
      out.write_element_begin("div",
                              HtmlElementOptions().set_class(page.classes));
      write_page_items(out, page.clip_defs, page.items, page.width, page.height,
                       write_line);
      out.write_element_end("div");
    }
    out.write_body_end();
    out.write_end();

    return resources;
  }

  static std::string pt_decl(const char *property, double value) {
    std::ostringstream s;
    s << property << ':' << value << "pt";
    return std::move(s).str();
  }

  /// Whether a run at (`ox`, `baseline`) starts a new visual line rather than
  /// continuing the previous run: its baseline jumped by more than 0.6× the
  /// previous run's advance height, or its origin sits left of the previous
  /// run's right edge (minus half that height) — a carriage return. Shared by
  /// all three layers (visual, selection, single) so the heuristic, which is
  /// always measured against the *previous* run's `prev_font_pt`, cannot drift
  /// between them. Callers gate on `prev_font_pt > 0`.
  static bool starts_new_line(const double baseline, const double prev_baseline,
                              const double ox, const double prev_end,
                              const double prev_font_pt) {
    return std::abs(baseline - prev_baseline) > 0.6 * prev_font_pt ||
           ox < prev_end - 0.5 * prev_font_pt;
  }

  /// The per-run geometry derived from a `TextElement` and the page's `to_box`
  /// transform. Identical in every text mode, so it lives in one place — no
  /// call site can compute it differently (that is how findings like the 180°
  /// rotation and the drifting line-break threshold crept in).
  struct RunGeometry {
    util::math::Transform2D m; ///< glyph space -> page box (y-down, pt later)
    bool invisible;            ///< Tr 3/7 — paints nothing, selectable only
    bool is_matrix;            ///< rotated/skewed/flipped -> CSS matrix path
    double asc;                ///< ascent in em (clamped)
    double scale;              ///< uniform axis scale (1 on the matrix path)
    double ox;                 ///< origin x (baseline left) in pt
    double baseline;           ///< origin y (baseline) in pt
    double extent;             ///< advance width in pt
    double font_pt;            ///< font size along the advance axis in pt
    double font_size_pt;       ///< CSS font-size in px
  };

  static RunGeometry run_geometry(const pdf::TextElement &text,
                                  const util::math::Transform2D &to_box) {
    constexpr util::math::Transform2D flip_glyph =
        util::math::Transform2D::scaling(1, -1);
    const util::math::Transform2D m = flip_glyph * text.transform * to_box;
    const bool invisible =
        text.rendering_mode == pdf::TextRenderingMode::invisible ||
        text.rendering_mode == pdf::TextRenderingMode::clip;
    // `m.a > 0` keeps the axis-aligned fast path from swallowing a pure 180°
    // rotation (a = d = -1, b = c = 0), which would otherwise feed a negative
    // `m.a` into `font_size_pt` and the left/top math.
    const bool is_matrix = !(m.b == 0 && m.c == 0 && m.a == m.d && m.a > 0);
    const double tz = text.horizontal_scaling / 100.0;
    const double axis = tz != 0 ? std::hypot(m.a, m.b) / tz : 0;
    return RunGeometry{
        .m = m,
        .invisible = invisible,
        .is_matrix = is_matrix,
        .asc = ascent_em(text),
        .scale = is_matrix ? 1.0 : m.a,
        .ox = m.e,
        .baseline = m.f,
        .extent = text.width * axis,
        .font_pt = text.size * axis,
        .font_size_pt = round2(is_matrix ? text.size : m.a * text.size),
    };
  }

  /// The colour class suffix (with a leading space) for a run's paint colour,
  /// or "" for black / invisible. Interns the declaration in `styles`. Shared
  /// by both modes' run emission.
  static std::string color_class(const pdf::TextElement &text,
                                 const bool invisible, AtomicStyles &styles) {
    if (invisible) {
      return {};
    }
    const pdf::GraphicsState::Color &paint =
        (text.rendering_mode == pdf::TextRenderingMode::stroke ||
         text.rendering_mode == pdf::TextRenderingMode::stroke_clip)
            ? text.stroke_color
            : text.fill_color;
    std::string css = device_color_to_css(paint);
    if (css == "rgb(0,0,0)") {
      return {};
    }
    return ' ' + styles.intern("k", "color:" + std::move(css));
  }

  /// The page-box geometry (dimensions, the page `to_box` transform and the
  /// `.p x# y#` class string) shared by both modes' page setup. `add_class`
  /// interns the width/height declarations.
  struct PageBox {
    double width;
    double height;
    util::math::Transform2D to_box;
    std::string classes;
  };

  template <typename AddClass>
  static PageBox begin_page(const pdf::Page &page, AddClass &&add_class) {
    const pdf::Array &page_box = page.media_box.as_array();
    const double box_x0 = page_box[0].as_real();
    const double box_y0 = page_box[1].as_real();
    const double width = page_box[2].as_real() - box_x0;
    const double height = page_box[3].as_real() - box_y0;

    std::string classes = "p";
    {
      std::ostringstream w;
      w << "width:" << width * pt_to_in << "in";
      add_class(classes, "x", std::move(w).str());
      std::ostringstream h;
      h << "height:" << height * pt_to_in << "in";
      add_class(classes, "y", std::move(h).str());
    }

    const util::math::Transform2D to_box =
        util::math::Transform2D::translation(-box_x0, -box_y0) *
        util::math::Transform2D::scaling_translation(1, -1, 0, height);

    return {width, height, to_box, std::move(classes)};
  }

  /// Returns the 1-based font family index for `font`, or 0 when it is unusable
  /// (or already rejected). On the first acceptance of a usable font runs
  /// `on_accept(index)` so the caller can grow its parallel per-font arrays.
  /// Shared accept/reject bookkeeping for both modes' `font_family` lambdas.
  template <typename OnAccept>
  static std::uint32_t intern_font(
      std::unordered_map<const pdf::Font *, std::uint32_t> &family_index,
      std::uint32_t &family_count, const pdf::Font *font,
      OnAccept &&on_accept) {
    const auto [it, inserted] = family_index.try_emplace(font, 0);
    if (!inserted) {
      return it->second;
    }
    if (!font_is_usable(*font)) {
      return 0;
    }
    const std::uint32_t index = ++family_count;
    it->second = index;
    on_accept(index);
    return index;
  }

  /// Writes a page's `<defs>` clips and its paint-order body — an SVG
  /// open/close dance around a `variant<LineT, PathT>` item list — via
  /// `write_line`. The structure is identical in both modes; only the line and
  /// path types and the line writer differ.
  template <typename LineT, typename PathT, typename WriteLine>
  static void
  write_page_items(HtmlWriter &out, const std::string &clip_defs,
                   const std::vector<std::variant<LineT, PathT>> &items,
                   const double width, const double height,
                   WriteLine &&write_line) {
    if (!clip_defs.empty()) {
      out.write_raw("<svg width=\"0\" height=\"0\" style=\"position:absolute\">"
                    "<defs>");
      out.write_raw(clip_defs);
      out.write_raw("</defs></svg>");
    }
    bool svg_open = false;
    const auto close_svg = [&] {
      if (svg_open) {
        out.write_raw("</svg>");
        svg_open = false;
      }
    };
    for (const std::variant<LineT, PathT> &item : items) {
      if (const auto *path = std::get_if<PathT>(&item)) {
        if (!svg_open) {
          std::ostringstream open;
          open << "<svg class=\"s\" viewBox=\"0 0 " << round2(width) << ' '
               << round2(height) << "\" preserveAspectRatio=\"none\">";
          out.write_raw(std::move(open).str());
          svg_open = true;
        }
        out.write_raw(path->svg);
      } else {
        close_svg();
        write_line(std::get<LineT>(item));
      }
    }
    close_svg();
  }

  /// Writes the document/head prologue shared by both modes: the constant
  /// `body`/`.p`/`.t` rules, then `write_mode_css()` for the mode-specific
  /// rules, then the constant `.s` rule, the font faces/styles and the interned
  /// atomic rules. Leaves the writer positioned after `</head>`.
  template <typename WriteModeCss>
  static void write_header_common(HtmlWriter &out,
                                  const std::string &font_faces,
                                  const std::string &font_styles,
                                  const AtomicStyles &styles,
                                  WriteModeCss &&write_mode_css) {
    out.write_begin();
    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    out.write_header_style_begin();
    out.out() << "body{margin:0;background:#525659}";
    out.out() << ".p{position:relative;margin:16px auto;background:#fff;"
                 "box-shadow:0 1px 4px rgba(0,0,0,.5)}";
    // `.t`: shared base for all absolutely-positioned line blocks.
    out.out() << ".t{position:absolute;left:0;top:0;transform-origin:0 0;"
                 "white-space:pre;line-height:1;font-kerning:none;"
                 "font-variant-ligatures:none}";
    write_mode_css();
    // SVG overlay covering the page box (visual graphics layer).
    out.out() << ".s{position:absolute;left:0;top:0;width:100%;height:100%;"
                 "overflow:hidden;pointer-events:none}";
    out.out() << font_faces;
    out.out() << font_styles;
    styles.write_rules(out.out());
    out.write_header_style_end();
    out.write_header_end();
  }

  /// Appends a text run's line-block placement classes via `add_class(classes,
  /// prefix, declaration)`: `l`/`t` (left/top, in pt) for an axis-aligned run,
  /// or `m` (a CSS `translate(...)` + `matrix(...)` transform, re-anchored to
  /// the run's baseline by `ascent_pt`) for a rotated/skewed one. A CSS
  /// `matrix()` translation is intrinsically px, so the (pt) translation is
  /// carried by a leading `translate(...pt)` — keeping the whole text layer in
  /// pt. Shared by the visual, selection and single-layer line blocks, which
  /// all position runs the same way.
  template <typename AddClass>
  static void add_position_classes(std::string &classes, AddClass &&add_class,
                                   const util::math::Transform2D &m,
                                   const bool is_matrix, const double ox,
                                   const double baseline,
                                   const double ascent_pt) {
    if (!is_matrix) {
      add_class(classes, "l", pt_decl("left", round2(ox)));
      add_class(classes, "t",
                pt_decl("top", round2(baseline - ascent_pt * m.a)));
      return;
    }
    // `translate()` accepts pt and is applied after (outside) the `matrix()`,
    // so `translate(tx,ty) matrix(a,b,c,d,0,0)` reproduces the full affine with
    // the translation authored in pt instead of matrix()'s implicit px.
    const double tx = m.e - m.c * ascent_pt;
    const double ty = m.f - m.d * ascent_pt;
    std::ostringstream t;
    t << "transform:translate(" << round2(tx) << "pt," << round2(ty)
      << "pt) matrix(" << m.a << ',' << m.b << ',' << m.c << ',' << m.d
      << ",0,0)";
    add_class(classes, "m", std::move(t).str());
  }

  /// Whether `font`'s embedded program can be re-encoded (SFNT PUA re-cmap or
  /// CFF->OTF wrap) without throwing; probes the real encode path so failures
  /// surface here rather than in the post-pass. Restores the SFNT's original
  /// cmap after probing (the CFF probe is stateless).
  static bool font_is_usable(const pdf::Font &font) {
    if (const auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
            font.embedded_font)) {
      std::map<char32_t, std::uint16_t> original_cmap = sfnt->cmap();
      bool usable = false;
      try {
        font::reencode_to_pua(*sfnt);
        std::ostringstream sfnt_out;
        sfnt->write(sfnt_out);
        usable = true;
      } catch (...) {
        usable = false;
      }
      sfnt->set_cmap(std::move(original_cmap));
      return usable;
    }
    if (const auto cff =
            std::dynamic_pointer_cast<font::cff::CffFont>(font.embedded_font)) {
      try {
        (void)font::cff::wrap_to_otf(*cff);
        return true;
      } catch (...) {
        return false;
      }
    }
    return false;
  }

  /// The `fvN`/`fnN` class for `font` (visible/invisible), marking it used in
  /// `font_class_used` so the post-pass emits the corresponding rule.
  static std::string
  font_class(std::vector<std::array<bool, 2>> &font_class_used,
             const std::uint32_t font, const bool inv) {
    font_class_used[font - 1][inv ? 1 : 0] = true;
    return (inv ? "fn" : "fv") + std::to_string(font);
  }

  /// Re-encodes `font`'s embedded program (SFNT PUA re-cmap or CFF->OTF wrap,
  /// folding in `extra_unicode`'s real-Unicode cmap entries alongside the PUA
  /// range) and appends its `@font-face` and `.fvN`/`.fnN` rules.
  /// `class_used[0]`/`[1]` gate whether the visible/invisible rule is needed.
  static void
  write_font_face(const pdf::Font &font, const std::uint32_t index,
                  const std::map<char32_t, std::uint16_t> &extra_unicode,
                  const std::array<bool, 2> &class_used,
                  std::string &font_faces, std::string &font_styles) {
    std::string reencoded;
    if (const auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
            font.embedded_font)) {
      font::reencode_to_pua(*sfnt, extra_unicode);
      std::ostringstream sfnt_out;
      sfnt->write(sfnt_out);
      reencoded = std::move(sfnt_out).str();
    } else if (const auto cff = std::dynamic_pointer_cast<font::cff::CffFont>(
                   font.embedded_font)) {
      reencoded = font::cff::wrap_to_otf(*cff, extra_unicode);
    }
    const std::string url = file_to_url(reencoded, "font/ttf");
    const std::string n = std::to_string(index + 1);
    font_faces += "@font-face{font-family:'odr-f";
    font_faces += n;
    font_faces += "';src:url(";
    font_faces += url;
    font_faces += ");}";
    const auto rule = [&](const char *cls, const char *color) {
      font_styles += '.';
      font_styles += cls;
      font_styles += n;
      font_styles += '{';
      font_styles += color;
      font_styles += "font-family:'odr-f";
      font_styles += n;
      font_styles += "'}";
    };
    if (class_used[0]) {
      rule("fv", "color:#000;");
    }
    if (class_used[1]) {
      rule("fn", "color:transparent;");
    }
  }

  static double ascent_em(const pdf::TextElement &text) {
    double em = 0.8;
    if (text.font != nullptr && text.font->descriptor_ascent) {
      em = *text.font->descriptor_ascent;
    } else if (text.font != nullptr && text.font->embedded_font != nullptr) {
      const std::uint16_t units = text.font->embedded_font->units_per_em();
      if (units != 0) {
        em = static_cast<double>(
                 text.font->embedded_font->bounding_box().y_max) /
             units;
      }
    }
    return std::clamp(em, 0.5, 1.2);
  }

  static std::string glyph_run_str(const pdf::Font &font,
                                   const std::string &codes) {
    std::string s;
    for (const std::uint32_t code : font.codes(codes)) {
      util::string::append_c32(font::pua_code_point(font.glyph_for_code(code)),
                               s);
    }
    return s;
  }

  /// Escapes only the three markup-significant characters for the selection /
  /// unicode text. Deliberately *not* `html::escape_text`: that substitutes
  /// spaces with `&nbsp;`, which browsers treat as a distinct character from
  /// U+0020 and so breaks find-in-page word matching and double-click word
  /// selection across the very text these layers exist to make selectable.
  static std::string escape_markup(std::string s) {
    util::string::replace_all(s, "&", "&amp;");
    util::string::replace_all(s, "<", "&lt;");
    util::string::replace_all(s, ">", "&gt;");
    return s;
  }

  /// Handles path/shading/image elements common to both rendering modes.
  /// Calls close_line() and push_fragment(svg_string) when a non-empty
  /// fragment is produced. Returns true when the element was a graphic
  /// (caller should `continue`), false when it is a text element.
  template <typename CloseLine, typename PushSvg>
  static bool handle_graphic_element(
      const pdf::PageElement &element, const util::math::Transform2D &to_box,
      double width, double height, ClipRegistry &clips,
      GradientRegistry &gradients, PatternRegistry &patterns, Logger &logger,
      CloseLine &&close_line, PushSvg &&push_svg) {
    if (const auto *path = std::get_if<pdf::PathElement>(&element)) {
      const std::string clip_id = clips.register_clip(path->clip, to_box);
      std::string fill_url_id;
      if (path->fill_shading != nullptr) {
        fill_url_id = gradients.register_gradient(
            *path->fill_shading, path->shading_transform * to_box);
      } else if (path->fill_pattern != nullptr) {
        fill_url_id = patterns.register_pattern(
            *path->fill_pattern, path->pattern_transform * to_box,
            path->fill_color, logger);
      }
      if (std::string frag =
              svg_path_fragment(*path, to_box, clip_id, fill_url_id);
          !frag.empty()) {
        close_line();
        push_svg(std::move(frag));
      }
      return true;
    }
    if (const auto *shading = std::get_if<pdf::ShadingElement>(&element)) {
      if (shading->shading != nullptr) {
        const std::string clip_id = clips.register_clip(shading->clip, to_box);
        const std::string gradient_id = gradients.register_gradient(
            *shading->shading, shading->transform * to_box);
        if (std::string frag =
                svg_shading_fragment(gradient_id, clip_id, width, height);
            !frag.empty()) {
          close_line();
          push_svg(std::move(frag));
        }
      }
      return true;
    }
    if (const auto *image = std::get_if<pdf::ImageElement>(&element)) {
      const std::string clip_id = clips.register_clip(image->clip, to_box);
      if (std::string frag = svg_image_fragment(*image, to_box, clip_id);
          !frag.empty()) {
        close_line();
        push_svg(std::move(frag));
      }
      return true;
    }
    return false;
  }

protected:
  PdfFile m_pdf_file;
  HtmlViews m_views;
};

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_pdf_service(const PdfFile &pdf_file,
                                     const std::string & /*cache_path*/,
                                     HtmlConfig config,
                                     std::shared_ptr<Logger> logger) {
  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      pdf_file, std::move(config), std::move(logger)));
}

} // namespace odr::internal
