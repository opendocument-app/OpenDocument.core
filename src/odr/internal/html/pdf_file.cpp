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

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odr::internal::html {

namespace {

/// Round to 0.01 user-space units; sub-precision beyond that is invisible and
/// the extra digits add up across a page full of path data.
double round2(const double v) { return std::round(v * 100.0) / 100.0; }

/// Escape only HTML markup (`&`, `<`, `>`) for the selection layer. Unlike
/// `html::escape_text`, spaces are left as ordinary U+0020 rather than
/// rewritten to `&nbsp;`: the selection spans carry `white-space:pre`, so every
/// space already renders, and a non-breaking space would defeat the layer's
/// purpose — it doesn't match a normal space in find-in-page and it glues
/// adjacent words together under double-click. Tabs aren't expected in
/// extracted PDF text.
std::string escape_selection_text(std::string text) {
  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");
  return text;
}

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

/// Convert a PDF device color to a CSS `rgb(...)` string. Non-device color
/// spaces (Separation/ICCBased/… — stage 4.4) and the unknown space fall back
/// to black, the PDF initial color.
std::string device_color_to_css(const pdf::GraphicsState::Color &color) {
  const auto to255 = [](const double v) {
    return static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
  };
  int r = 0;
  int g = 0;
  int b = 0;
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
  const auto to255 = [](const double v) {
    return static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
  };
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
    const bool dashed =
        std::any_of(path.dash_array.begin(), path.dash_array.end(),
                    [](const double v) { return v > 0; });
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
  // image natural box [0,1] (y-down) -> PDF unit square (y-up) -> user -> box.
  constexpr util::math::Transform2D flip =
      util::math::Transform2D::scaling_translation(1, -1, 0, 1);
  const util::math::Transform2D m = flip * image.transform * to_box;

  std::ostringstream f;
  // The clip wraps the image in a transform-free `<g>` rather than sitting on
  // the `<image>`: see the function comment.
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
    const util::math::Transform2D identity;
    std::ostringstream tile;
    for (const pdf::PageElement &element :
         pdf::extract_page(pattern.content, *pattern.resources, logger)) {
      if (const auto *path = std::get_if<pdf::PathElement>(&element)) {
        pdf::PathElement painted = *path;
        if (uncoloured) {
          painted.fill_color = fill_color;
          painted.stroke_color = fill_color;
        }
        tile << svg_path_fragment(painted, identity, "", "");
      } else if (const auto *image = std::get_if<pdf::ImageElement>(&element)) {
        tile << svg_image_fragment(*image, identity, "");
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
/// emits one absolutely-positioned span per glyph run, and the same font sizes,
/// offsets and spacings recur across the (potentially millions of) spans.
/// Writing each declaration inline bloats the document — the Bluetooth Core
/// spec reference output crossed GitHub's 100 MB file limit. Instead, every
/// distinct declaration is registered once here, named `<prefix><n>` in
/// first-seen order (e.g. `f1`, `f2` for font sizes, `t1` for a top offset),
/// emitted once in <head>, and referenced by class on each span. This is
/// representation-only: the computed style of every element is unchanged.
class AtomicStyles {
public:
  /// `prefix` selects the property family; `declaration` is a full CSS
  /// declaration without trailing ';' (e.g. "font-size:13.28px"). Returns the
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

  /// Writes one rule per line (`.f1{font-size:13.28px}`) so regeneration diffs
  /// stay legible. Each rule is preceded by a newline; the caller has already
  /// written the constant rules on the opening `<style>` line.
  void write_rules(std::ostream &o) const {
    for (const auto *entry : m_order) {
      o << "\n." << entry->second << '{' << entry->first << '}';
    }
  }

private:
  // Node-based map: pointers stored in `m_order` stay valid across insertions.
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

  // One emitted span: the resolved class tokens plus the already-escaped text.
  // The renderer paints text in two independent layers (see `write_document`):
  // the **visual** layer (`PageOut::items`, in paint order) carries the
  // unselectable glyphs; the **selection** layer (`PageOut::sel_spans`, in
  // content/reading order) carries the transparent, selectable real Unicode.
  // Both layers are flat — a span is just classes + text.
  struct SpanOut {
    std::string classes;
    std::string text;
  };
  // One vector item, already serialized to an SVG fragment in the page's
  // viewBox (PDF points, y-down): a painted `<path>` or an `<image>`.
  // Contiguous vector items share one `<svg>` at write time.
  struct PathOut {
    std::string svg;
  };
  // Visual page content in paint (z) order: glyph spans and paths interleave,
  // so a later fill occludes earlier text and vice versa.
  using PageItem = std::variant<SpanOut, PathOut>;
  struct PageOut {
    std::string classes;
    double width{0};  // page box width, PDF points (for the SVG viewBox)
    double height{0}; // page box height, PDF points
    std::vector<PageItem> items;
    // The selection layer: transparent, selectable Unicode spans in
    // content-stream (reading) order, emitted after the visual content so they
    // form one contiguous, cleanly selectable run in the DOM.
    std::vector<SpanOut> sel_spans;
    // `<clipPath>` defs for this page's clipped paths, emitted once in a hidden
    // `<svg>`; the path fragments reference them by id. Empty when no path on
    // the page is clipped.
    std::string clip_defs;
  };

  HtmlResources write_document(HtmlWriter &out) const {
    HtmlResources resources;

    const auto &pdf_file =
        dynamic_cast<const pdf::PdfFile &>(*m_pdf_file.impl());
    pdf::DocumentParser parser = pdf_file.create_parser(*m_logger);
    const std::unique_ptr<pdf::Document> document = parser.parse_document();

    const std::vector<pdf::Page *> pages = document->collect_pages();

    // CSS uses 96px to the inch, PDF user space 72 units to the inch.
    static constexpr double pt_to_px = 96.0 / 72.0;
    static constexpr double pt_to_in = 1 / 72.0;

    // Pass 1: resolve every page and span into class tokens, building the
    // atomic-style catalog so it can be emitted in <head> ahead of the body.
    AtomicStyles styles;
    std::vector<PageOut> pages_out;
    pages_out.reserve(pages.size());

    // Embedded fonts get a PUA re-encode, an `@font-face`, and a `font-family`
    // class `odr-fN`, assigned on first encounter in `font_family`. A font
    // whose embedded font is absent, not an SFNT, or not re-encodable keeps
    // index 0 and renders through the fallback path, exactly as before.
    //
    // The `@font-face` rules are *not* built here: the font subset isn't needed
    // until the post-pass, which re-encodes each accepted font to the PUA and
    // emits `font_faces`. `font_family` therefore just validates and indexes
    // the font; `accepted_fonts` (indexed by `index - 1`) carries it forward.
    std::uint32_t family_count = 0;
    std::string font_faces;
    std::string glyph_styles; // per-font visible-glyph class `.fvN`
    std::vector<pdf::Font *> accepted_fonts;
    std::unordered_map<const pdf::Font *, std::uint32_t> family_index;
    const auto font_family = [&](pdf::Font *font) -> std::uint32_t {
      const auto [it, inserted] = family_index.try_emplace(font, 0);
      if (!inserted) {
        return it->second; // already classified
      }
      // Gate the embedded-font path on a trial PUA re-encode + serialize: a
      // font we cannot re-encode (more glyphs than the BMP PUA holds, or a
      // serialization failure) keeps index 0 and renders through the legible
      // fallback path, never emitting orphaned PUA glyph spans. The trial
      // output is discarded; the post-pass re-encodes for real (with the
      // real-Unicode entries) once the used-scalar set is known.
      bool usable = false;
      if (auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
              font->embedded_font)) {
        // SFNT (TrueType / OpenType): the re-encode mutates the cmap in place,
        // but `glyph_for_code` reads it during extraction below, so snapshot
        // the original cmap and restore it after.
        std::map<char32_t, std::uint16_t> original_cmap = sfnt->cmap();
        try {
          font::reencode_to_pua(*sfnt);
          std::ostringstream sfnt_out;
          sfnt->write(sfnt_out);
          usable = true;
        } catch (...) {
          usable = false;
        }
        sfnt->set_cmap(std::move(original_cmap));
      } else if (auto cff = std::dynamic_pointer_cast<font::cff::CffFont>(
                     font->embedded_font)) {
        // Bare CFF (`/FontFile3`): wrap into an OTTO with the PUA cmap baked in
        // (no in-place mutation, so nothing to restore).
        try {
          (void)font::cff::wrap_to_otf(*cff);
          usable = true;
        } catch (...) {
          usable = false;
        }
      }
      if (!usable) {
        return 0; // no usable embedded font: fallback path
      }
      const std::uint32_t index = ++family_count;
      it->second = index;
      accepted_fonts.push_back(font);
      return index;
    };

    // The per-font visible-glyph class `.fvN`, carrying `font-family:'odr-fN'`
    // and the black paint, so a glyph span names one class instead of restating
    // the font family on every one of the (potentially millions of) spans.
    const auto font_class = [](const std::uint32_t font) {
      return "fv" + std::to_string(font);
    };

    // The PUA glyph string for a run: each character code -> glyph id ->
    // deterministic PUA code point (`U+E000 + glyph`), matching the re-encode.
    const auto glyph_run = [](const pdf::Font &font, const std::string &codes) {
      std::string new_codes;
      for (const std::uint32_t code : font.codes(codes)) {
        util::string::append_c32(
            font::pua_code_point(font.glyph_for_code(code)), new_codes);
      }
      return new_codes;
    };

    // Appends `prefix:value` (interned) as a class token on `classes`.
    const auto add_class = [&styles](std::string &classes,
                                     const std::string &prefix,
                                     std::string declaration) {
      classes += ' ';
      classes += styles.intern(prefix, std::move(declaration));
    };
    const auto px_decl = [](const char *property, const double value) {
      std::ostringstream s;
      s << property << ':' << value << "px";
      return std::move(s).str();
    };

    // A run's baseline-to-top distance, in em, so it can be placed by its
    // baseline (PDF's text origin) rather than by its CSS box top — which is
    // one ascent above the baseline. Prefers the FontDescriptor `/Ascent`, then
    // the embedded font's bounding box, then a typical 0.8 em. Clamped to a
    // sane band so a degenerate metric cannot fling a run off the page. Paired
    // with `line-height:1` on the spans, the box top sits ~one ascent above the
    // baseline (exact when ascent + descent == 1 em, near so otherwise), so
    // subtracting the ascent lands the baseline on the PDF origin.
    const auto ascent_em = [](const pdf::TextElement &text) -> double {
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
    };

    for (pdf::Page *page : pages) {
      const pdf::Array &page_box = page->media_box.as_array();
      const double box_x0 = page_box[0].as_real();
      const double box_y0 = page_box[1].as_real();
      const double width = page_box[2].as_real() - box_x0;
      const double height = page_box[3].as_real() - box_y0;

      PageOut &page_out = pages_out.emplace_back();
      page_out.classes = "p";
      page_out.width = width;
      page_out.height = height;
      {
        std::ostringstream w;
        w << "width:" << width * pt_to_in << "in";
        add_class(page_out.classes, "x", std::move(w).str());
        std::ostringstream h;
        h << "height:" << height * pt_to_in << "in";
        add_class(page_out.classes, "y", std::move(h).str());
      }

      std::string stream;
      for (const auto &content_reference : page->contents_reference) {
        // streams of one page join at token boundaries (ISO 32000-1 7.7.3.3)
        stream += parser.read_decoded_stream(content_reference);
        stream += '\n';
      }

      // Map PDF user space (origin at the MediaBox corner, y up) to the page
      // box in CSS pixels (origin top-left, y down). `flip_glyph` un-mirrors
      // the glyphs so text stays upright after the page flip.
      constexpr util::math::Transform2D flip_glyph =
          util::math::Transform2D::scaling(1, -1);
      const util::math::Transform2D to_box =
          util::math::Transform2D::translation(-box_x0, -box_y0) *
          util::math::Transform2D::scaling_translation(1, -1, 0, height);

      ClipRegistry clips(static_cast<std::uint32_t>(pages_out.size()));
      GradientRegistry gradients(static_cast<std::uint32_t>(pages_out.size()));
      PatternRegistry patterns(static_cast<std::uint32_t>(pages_out.size()));

      // Selection-layer grouping sweep state, in content-stream (reading)
      // order. Tracks the previous text run's baseline and right edge (page-box
      // points, y down) so the next run can be prefixed with a separator space
      // on a line/column break or a wide intra-line gap.
      bool have_prev_run = false;
      double prev_baseline = 0;
      double prev_end = 0;
      bool prev_ends_space = false;

      for (const pdf::PageElement &element :
           pdf::extract_page(stream, *page->resources, *m_logger)) {
        // A painted path: serialize its subpaths to an SVG `<path>` fragment in
        // the page viewBox (fill and/or stroke), under any active clip. A
        // shading- or tiling-pattern fill is painted through a paint server
        // (gradient/`<pattern>`) instead of a colour.
        if (const auto *path = std::get_if<pdf::PathElement>(&element);
            path != nullptr) {
          const std::string clip_id = clips.register_clip(path->clip, to_box);
          std::string fill_url_id;
          if (path->fill_shading != nullptr) {
            fill_url_id = gradients.register_gradient(
                *path->fill_shading, path->shading_transform * to_box);
          } else if (path->fill_pattern != nullptr) {
            fill_url_id = patterns.register_pattern(
                *path->fill_pattern, path->pattern_transform * to_box,
                path->fill_color, *m_logger);
          }
          std::string fragment =
              svg_path_fragment(*path, to_box, clip_id, fill_url_id);
          if (!fragment.empty()) {
            page_out.items.push_back(PathOut{std::move(fragment)});
          }
          continue;
        }

        // An `sh` shading flood: a `<rect>` over the page box filled with the
        // shading's gradient, bounded by the clip in force at `sh` time.
        if (const auto *shading = std::get_if<pdf::ShadingElement>(&element);
            shading != nullptr) {
          if (shading->shading == nullptr) {
            continue;
          }
          const std::string clip_id =
              clips.register_clip(shading->clip, to_box);
          const std::string gradient_id = gradients.register_gradient(
              *shading->shading, shading->transform * to_box);
          std::string fragment =
              svg_shading_fragment(gradient_id, clip_id, width, height);
          if (!fragment.empty()) {
            page_out.items.push_back(PathOut{std::move(fragment)});
          }
          continue;
        }

        // An image XObject: an `<image>` placed by the CTM, in the page `<svg>`
        // alongside the paths (so it layers by paint order).
        if (const auto *image = std::get_if<pdf::ImageElement>(&element)) {
          const std::string clip_id = clips.register_clip(image->clip, to_box);
          std::string fragment = svg_image_fragment(*image, to_box, clip_id);
          if (!fragment.empty()) {
            page_out.items.push_back(PathOut{std::move(fragment)});
          }
          continue;
        }

        const pdf::TextElement &text = std::get<pdf::TextElement>(element);
        // TODO(clip text): the active clip is not applied to text. Paths carry
        // a clip snapshot realized as an SVG `<clipPath>`, but text is emitted
        // as HTML spans that the clipPath cannot reach, so clipped text paints
        // outside its region. See STAGE4_PLAN.md "4.3 — Clipping" follow-up.
        // The font index is non-zero when an embedded font lets us render
        // the actual glyphs; 0 falls through to the legacy path.
        const std::uint32_t font =
            text.font != nullptr ? font_family(text.font) : 0;
        // Without an embedded font, an empty `text` has nothing to show: a code
        // with no recoverable Unicode (`no_unicode`) or an `/ActualText`-
        // suppressed segment. With one, the glyphs still render (PUA layer).
        if (text.text.empty() && font == 0) {
          continue;
        }

        const util::math::Transform2D m = flip_glyph * text.transform * to_box;

        // Tr 3 (invisible) and Tr 7 (clip-only) paint nothing; they emit no
        // visual span at all and survive only in the selection layer (so
        // OCR-over-scan text stays searchable/selectable).
        const bool invisible =
            text.rendering_mode == pdf::TextRenderingMode::invisible ||
            text.rendering_mode == pdf::TextRenderingMode::clip;

        // The run's visible paint colour, folded onto the visual glyph span as
        // an interned colour class — but only when it is not the default black,
        // so the overwhelmingly common black run adds nothing. The per-font
        // `.fvN` class declares `color:#000`; this class is emitted after it in
        // <head> (equal specificity), so it overrides. The selection layer is
        // transparent and takes no colour. The fill modes paint with the
        // non-stroking colour, the stroke-only modes (Tr 1/5) with the stroking
        // colour.
        std::string color_suffix;
        if (!invisible) {
          const pdf::GraphicsState::Color &paint =
              (text.rendering_mode == pdf::TextRenderingMode::stroke ||
               text.rendering_mode == pdf::TextRenderingMode::stroke_clip)
                  ? text.stroke_color
                  : text.fill_color;
          if (std::string css = device_color_to_css(paint);
              css != "rgb(0,0,0)") {
            color_suffix = ' ' + styles.intern("k", "color:" + std::move(css));
          }
        }

        // Placement and spacing are shared by both layers of a run; build them
        // once on `base`.
        std::string base = "t";

        // Place by the baseline: PDF's text origin (`m.e`, `m.f`) is the glyph
        // baseline, but a CSS span anchors its box top, which sits one ascent
        // above the baseline. Shift the origin up by the ascent along the run's
        // local y axis so the baseline lands on the PDF origin.
        const double asc = ascent_em(text);

        // Tc/Tw are absolute text-space lengths (not scaled by the font size).
        // One text-space unit is `scale * pt_to_px` CSS px, where `scale` is
        // the linear factor we apply to the glyphs: folded into `font-size` in
        // the uniform branch, carried by the CSS matrix in the general branch
        // (so spacing there is expressed pre-transform, scale == 1).
        double scale;
        if (m.b == 0 && m.c == 0 && m.a == m.d) {
          // Upright uniform scale: fold the scale into the font size and place
          // the origin with left/top, so the (otherwise near-universal) matrix
          // is dropped. The ascent shift is purely vertical here (local y maps
          // to box y, scaled by `m.a`).
          add_class(base, "l", px_decl("left", round2(m.e * pt_to_px)));
          add_class(
              base, "t",
              px_decl("top", round2((m.f - asc * m.a * text.size) * pt_to_px)));
          add_class(base, "f",
                    px_decl("font-size", round2(m.a * text.size * pt_to_px)));
          scale = m.a;
        } else {
          // The ascent shift is `asc` em down the local y axis, whose direction
          // in the box is the matrix's (c, d) column; subtract it from the
          // translation so the baseline, not the box top, lands on the origin.
          const double ascent_px = asc * text.size * pt_to_px;
          std::ostringstream tm;
          tm << "transform:matrix(" << m.a << "," << m.b << "," << m.c << ","
             << m.d << "," << round2(m.e * pt_to_px - m.c * ascent_px) << ","
             << round2(m.f * pt_to_px - m.d * ascent_px) << ")";
          add_class(base, "m", std::move(tm).str());
          add_class(base, "f",
                    px_decl("font-size", round2(text.size * pt_to_px)));
          scale = 1;
        }

        // PDF char/word spacing (Tc/Tw) translate directly to CSS. TJ kerning
        // needs no expression here: `extract_text` emits a separate segment per
        // TJ string and folds the adjustment into the following segment's
        // `transform`, so a segment only carries its constant spacing. Emitted
        // only when non-zero to keep the (overwhelmingly common) unspaced span
        // small.
        //
        // CSS letter-/word-spacing key on the *rendered* string's character and
        // space boundaries, but PDF Tc/Tw advance the text matrix per raw code
        // (Tw only on a simple font's single-byte 0x20; ISO 32000-1 9.3.3). The
        // two coincide only when the rendered run is 1:1 with the codes. The
        // glyph layer always is (one PUA code point per code, `font != 0`); the
        // Unicode text layer is not when a /ToUnicode CMap expands a code into
        // several characters (ligatures), /ActualText substitutes text, or a
        // space was inferred — there CSS would insert gaps the segment advances
        // never accounted for, splitting glyphs and drifting the next
        // absolutely-positioned segment. Gate emission on that correspondence;
        // word spacing additionally never applies to a composite font.
        const bool spacing_one_to_one =
            font != 0 ||
            (text.font != nullptr &&
             util::string::utf8_length(text.text) == text.advances.size());
        if (text.char_spacing != 0 && spacing_one_to_one) {
          add_class(base, "s",
                    px_decl("letter-spacing",
                            round2(text.char_spacing * scale * pt_to_px)));
        }
        if (text.word_spacing != 0 && spacing_one_to_one &&
            !(text.font != nullptr && text.font->composite)) {
          add_class(base, "w",
                    px_decl("word-spacing",
                            round2(text.word_spacing * scale * pt_to_px)));
        }

        // --- Selection layer -------------------------------------------------
        // Every run with extractable text feeds the transparent, selectable
        // layer (`.i`) with its real Unicode, anchored at the run origin via
        // the shared placement (`base`). A content-order sweep decides, per
        // run, whether it starts a new span or extends the previous one:
        //
        //  * A line/column break or a wide intra-line gap starts a new span,
        //    prefixed with a separator space so search and copy get whitespace
        //    across the boundary. The space is suppressed when either side
        //    already carries whitespace — a double space breaks literal
        //    find-in-page, and inter-word gaps often already left an inferred
        //    leading space on `text.text`.
        //  * A tight same-baseline continuation with no whitespace at the
        //    boundary merges into the previous span. PDF splits one word into
        //    several runs at every TJ kerning adjustment, and the browser finds
        //    word boundaries within a single text node only, so a word spread
        //    over separate spans can't be grown by a double-click. Folding the
        //    continuation into the previous text node keeps the whole word
        //    selectable as a unit. A boundary that already carries a space is a
        //    word break, not an intra-word split, so it stays a separate span —
        //    gluing the words into one node (over a non-breaking separator)
        //    would instead make a double-click grab the whole phrase. The
        //    merged run's own origin is dropped — its glyphs flow from where
        //    the previous run ended — but the runs are tightly packed by
        //    construction and the layer is transparent, so the sub-glyph drift
        //    is invisible.
        if (!text.text.empty()) {
          // Run origin and horizontal extent in page-box points (y down). The
          // advance (`text.width`) lives in the text matrix's space; its box
          // extent scales by the text-matrix -> box x-axis length. The
          // placement transform's x-axis (`m.a`, `m.b`) additionally folds in
          // horizontal scaling (Tz), but `text.width` already advanced with Tz
          // in `segment_advances`; divide it back out so Tz is applied once
          // (and so `font_pt` tracks the Tz-free em).
          const double ox = m.e;
          const double baseline = m.f;
          const double tz = text.horizontal_scaling / 100.0;
          const double axis = tz != 0 ? std::hypot(m.a, m.b) / tz : 0;
          const double extent = text.width * axis;
          const double font_pt = text.size * axis;
          const bool starts_space = text.text.front() == ' ';

          bool merge = false;
          std::string sep;
          if (have_prev_run && font_pt > 0) {
            const bool new_line =
                std::abs(baseline - prev_baseline) > 0.6 * font_pt ||
                ox < prev_end - 0.5 * font_pt;
            const bool gap = ox - prev_end > 0.25 * font_pt;
            const bool boundary_space = prev_ends_space || starts_space;
            if (new_line || gap) {
              if (!boundary_space) {
                sep = " ";
              }
            } else if (!boundary_space) {
              merge = true;
            }
          }

          if (merge && !page_out.sel_spans.empty()) {
            page_out.sel_spans.back().text += escape_selection_text(text.text);
          } else {
            page_out.sel_spans.push_back(
                SpanOut{base + " i", escape_selection_text(sep + text.text)});
          }

          prev_baseline = baseline;
          prev_end = ox + extent;
          prev_ends_space = text.text.back() == ' ';
          have_prev_run = true;
        }

        // --- Visual layer ----------------------------------------------------
        // Unselectable glyphs in paint order. Invisible runs (Tr 3/7) paint
        // nothing, so they emit no visual span — they live only in the
        // selection layer above.
        if (!invisible) {
          if (font != 0) {
            // PUA code points in the embedded font, carrying the placement
            // (`base`), `.g` (unselectable) and the per-font paint+family
            // class.
            std::string classes = base + " g ";
            classes += font_class(font);
            classes += color_suffix;
            page_out.items.push_back(
                SpanOut{std::move(classes),
                        escape_text(glyph_run(*text.font, text.codes))});
          } else {
            // No embedded font: render the Unicode in a fallback font,
            // unselectable (the selection layer owns interaction).
            std::string classes = base + " g";
            classes += color_suffix;
            page_out.items.push_back(
                SpanOut{std::move(classes), escape_text(text.text)});
          }
        }
      }

      // Clip-path, gradient and pattern defs share the page's hidden
      // `<svg><defs>`.
      page_out.clip_defs = clips.defs() + gradients.defs() + patterns.defs();
    }

    // Post-pass: re-encode each accepted font to the PUA and emit its
    // `@font-face` rule plus the per-font visible-glyph class in index order,
    // so the output stays deterministic. The visual glyph layer renders PUA
    // code points only (selection rides the separate transparent layer), so no
    // real-Unicode `cmap` entries are baked.
    for (std::uint32_t i = 0; i < family_count; ++i) {
      pdf::Font *font = accepted_fonts[i];
      std::string reencoded;
      if (auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
              font->embedded_font)) {
        font::reencode_to_pua(*sfnt);
        std::ostringstream sfnt_out;
        sfnt->write(sfnt_out);
        reencoded = std::move(sfnt_out).str();
      } else if (auto cff = std::dynamic_pointer_cast<font::cff::CffFont>(
                     font->embedded_font)) {
        reencoded = font::cff::wrap_to_otf(*cff);
      }
      const std::string url = file_to_url(reencoded, "font/ttf");
      const std::string n = std::to_string(i + 1);
      font_faces += "@font-face{font-family:'odr-f";
      font_faces += n;
      font_faces += "';src:url(";
      font_faces += url;
      font_faces += ");}";

      // `.fvN` carries the font family and the black paint; placement (`.t`),
      // unselectability (`.g`) and any non-black colour stay on the span.
      glyph_styles += ".fv";
      glyph_styles += n;
      glyph_styles += "{color:#000;font-family:'odr-f";
      glyph_styles += n;
      glyph_styles += "'}";
    }

    // Pass 2: write the document, now that the catalog is complete.
    out.write_begin();
    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    // Constant per-page and per-glyph styling lives in classes so it is not
    // repeated inline on every one of the (potentially millions of) spans.
    out.write_header_style_begin();
    // Page presentation: a neutral backdrop with each page as a centered white
    // sheet and a soft drop shadow, mirroring the familiar PDF-viewer look.
    // This is purely cosmetic chrome around the page box; the
    // absolutely-positioned spans inside are unaffected (they anchor to the
    // `.p` box, which keeps `position:relative`).
    out.out() << "body{margin:0;background:#525659}";
    out.out() << ".p{position:relative;margin:16px auto;background:#fff;"
                 "box-shadow:0 1px 4px rgba(0,0,0,.5)}";
    // `font-kerning:none` + `font-variant-ligatures:none` keep the browser from
    // applying the embedded font's GPOS/GSUB tables: the PUA glyph layer
    // carries exact PDF glyph IDs and advances, and ligature substitution /
    // kerning would re-shape it, shifting pixels and run widths. Shared by both
    // layers
    // (`.t`).
    // `line-height:1` fixes the box top one em-ascent above the baseline so the
    // baseline shift applied to each run's `top`/matrix (see `ascent_em`) lands
    // the glyphs on the PDF text origin; the browser's default `normal` leading
    // would add an unknown offset.
    out.out() << ".t{position:absolute;left:0;top:0;transform-origin:0 0;"
                 "white-space:pre;line-height:1;font-kerning:none;"
                 "font-variant-ligatures:none}";
    // The selection layer: transparent (painted by the glyph layer underneath)
    // but selectable and searchable, including OCR-over-scan invisible text.
    out.out() << ".i{color:transparent}";
    // The visual glyph layer is not selectable — selection rides the `.i`
    // layer, so the (often PUA) visible code points stay off the clipboard.
    out.out() << ".g{user-select:none}";
    // Vector graphics: one or more `<svg>` overlays per page, each filling the
    // page box (viewBox in PDF points). `overflow:hidden` clips each overlay to
    // the page box, matching a PDF viewer: content drawn outside the MediaBox
    // (e.g. a background rectangle that bleeds past the left edge) is never
    // visible, and without this it spills into the centered page's margin.
    // In-page clip paths are honoured via per-path `clip-path` (the page's
    // `<clipPath>` defs are emitted in a hidden `<svg>` above).
    // `preserveAspectRatio:none` keeps the points->box mapping exact.
    // `pointer-events:none` so a full-page overlay painted after text
    // (paint order) does not swallow selection/clicks over its transparent
    // areas — the graphics are decorative, the text layer owns interaction.
    out.out() << ".s{position:absolute;left:0;top:0;width:100%;height:100%;"
                 "overflow:hidden;pointer-events:none}";
    // Embedded fonts, re-encoded to the PUA and served inline.
    out.out() << font_faces;
    // Per-font visible-glyph classes (`.fvN` paint+font family), so a glyph
    // span names one class for its font.
    out.out() << glyph_styles;
    // Per-value atomic classes (font sizes, offsets, transforms, ...). Shared
    // by the visual glyph layer and the selection layer (both anchor at the run
    // origin via these placement classes).
    styles.write_rules(out.out());
    out.write_header_style_end();
    out.write_header_end();

    const auto write_span = [&out](const SpanOut &span) {
      // Inline so the run stays on one line: smaller output and a more legible
      // diff than the open/text/close split, while each run still gets its own
      // line under the page div.
      out.write_element_begin(
          "span",
          HtmlElementOptions().set_inline(true).set_class(span.classes));
      out.write_raw(span.text);
      out.write_element_end("span");
    };

    out.write_body_begin();
    for (const PageOut &page : pages_out) {
      out.write_element_begin("div",
                              HtmlElementOptions().set_class(page.classes));

      // Visual layer: the page's graphics and unselectable glyphs, grouped in
      // one parent and hidden from the accessibility tree (`aria-hidden`) — the
      // glyphs are often PUA code points a screen reader would read as
      // gibberish, and the real text is carried by the selection layer below.
      // The wrapper is unpositioned and contributes no height (its children are
      // `position:absolute`), so it stays layout-neutral and the spans still
      // anchor to the `.p` page box.
      out.write_element_begin("div",
                              HtmlElementOptions().set_class("vis").set_extra(
                                  R"(aria-hidden="true")"));
      // Clip-path, gradient and pattern defs for this page, in a hidden
      // zero-size `<svg>`. They are referenced by id from the page's fragments;
      // `clipPathUnits`/`gradientUnits` are `userSpaceOnUse`, so the geometry
      // is read in the user space of the referencing element (the page
      // viewBox), not this `<svg>`.
      if (!page.clip_defs.empty()) {
        out.write_raw(
            "<svg width=\"0\" height=\"0\" style=\"position:absolute\">"
            "<defs>");
        out.write_raw(page.clip_defs);
        out.write_raw("</defs></svg>");
      }
      // Walk the page's elements in paint order, coalescing contiguous paths
      // into a single `<svg>` so spans and vector graphics layer by DOM order.
      bool svg_open = false;
      const auto close_svg = [&] {
        if (svg_open) {
          out.write_raw("</svg>");
          svg_open = false;
        }
      };
      for (const PageItem &item : page.items) {
        if (const auto *path = std::get_if<PathOut>(&item)) {
          if (!svg_open) {
            std::ostringstream open;
            open << "<svg class=\"s\" viewBox=\"0 0 " << round2(page.width)
                 << ' ' << round2(page.height)
                 << "\" preserveAspectRatio=\"none\">";
            out.write_raw(std::move(open).str());
            svg_open = true;
          }
          out.write_raw(path->svg);
        } else {
          close_svg();
          write_span(std::get<SpanOut>(item));
        }
      }
      close_svg();
      out.write_element_end("div");

      // Selection layer: transparent, selectable Unicode in reading order,
      // grouped in its own parent and emitted after the visual layer so the
      // spans are contiguous in the DOM and a drag- or find-selection flows
      // cleanly across runs and lines without the visual glyphs (which are
      // `user-select:none`) interrupting it.
      out.write_element_begin("div", HtmlElementOptions().set_class("sel"));
      for (const SpanOut &span : page.sel_spans) {
        write_span(span);
      }
      out.write_element_end("div");

      out.write_element_end("div");
    }
    out.write_body_end();
    out.write_end();

    return resources;
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
