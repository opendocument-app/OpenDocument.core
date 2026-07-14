#include <odr/internal/html/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/font.hpp>
#include <odr/internal/common/path.hpp>
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
#include <functional>
#include <map>
#include <memory>
#include <mutex>
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

/// One resolved link annotation, positioned in page-box points (y-down).
struct LinkOut {
  double left{0};
  double top{0};
  double width{0};
  double height{0};
  std::string href;     ///< external URI or an internal target (a "#pN" anchor
                        ///< or a "pageN.html" page view); already attr-escaped
  bool internal{false}; ///< true for an internal target (needs
                        ///< `target="_self"`)
};

/// Maps a link's 0-based target page index to the href navigating to it: a
/// "#pN" anchor in the combined document, a page-view file name in a
/// standalone page. Returns "" to drop the link (target page not rendered).
using PageHref = std::function<std::string(std::size_t)>;

/// Resolves a link annotation's destination to a page: a `page-object ->
/// 0-based index` map plus the catalog's named-destination table (`/Dests`
/// dictionary and the `/Names /Dests` name tree, ISO 32000-1 12.3.2.3).
struct LinkResolver {
  pdf::DocumentParser &parser;
  std::map<pdf::ObjectReference, std::size_t> page_index;
  std::map<std::string, pdf::Object> named_dests; ///< name -> raw dest value

  /// A destination (a name/string, a `[page …]` array, or a dict with `/D`)
  /// to its target page index, if it resolves to a known page.
  [[nodiscard]] std::optional<std::size_t> resolve_dest_page(pdf::Object dest) {
    dest = parser.resolve_object_copy(std::move(dest));
    if (dest.is_string() || dest.is_name()) {
      const std::string name =
          dest.is_string() ? dest.as_string() : dest.as_name();
      const auto it = named_dests.find(name);
      if (it == named_dests.end()) {
        return std::nullopt;
      }
      dest = parser.resolve_object_copy(it->second);
    }
    if (dest.is_dictionary() && dest.as_dictionary().has_value("D")) {
      dest = parser.resolve_object_copy(dest.as_dictionary().get("D"));
    }
    if (!dest.is_array() || dest.as_array().empty()) {
      return std::nullopt;
    }
    const pdf::Object &target = dest.as_array()[0];
    if (!target.is_reference()) {
      return std::nullopt;
    }
    const auto it = page_index.find(target.as_reference());
    return it != page_index.end() ? std::optional<std::size_t>(it->second)
                                  : std::nullopt;
  }
};

/// Walk a destination name tree (`Names` leaf pairs, `Kids` intermediates),
/// depth-guarded, collecting `name -> dest` into `out`.
void collect_dest_name_tree(pdf::DocumentParser &parser,
                            const pdf::Object &node_ref,
                            std::map<std::string, pdf::Object> &out,
                            const int depth) {
  if (depth > 50) {
    return;
  }
  const pdf::Object node = parser.resolve_object_copy(node_ref);
  if (!node.is_dictionary()) {
    return;
  }
  const pdf::Dictionary &dictionary = node.as_dictionary();
  if (dictionary.has_value("Names")) {
    const pdf::Array names =
        parser.resolve_object_copy(dictionary.get("Names")).as_array();
    for (std::size_t i = 0; i + 1 < names.size(); i += 2) {
      const pdf::Object key = parser.resolve_object_copy(names[i]);
      if (key.is_string()) {
        out.emplace(key.as_string(), names[i + 1]);
      }
    }
  }
  if (dictionary.has_value("Kids")) {
    const pdf::Array kids =
        parser.resolve_object_copy(dictionary.get("Kids")).as_array();
    for (const pdf::Object &kid : kids) {
      collect_dest_name_tree(parser, kid, out, depth + 1);
    }
  }
}

/// Build the link resolver once per document: the page-index map and the
/// catalog's named destinations.
LinkResolver build_link_resolver(pdf::DocumentParser &parser,
                                 const pdf::Document &document,
                                 const std::vector<pdf::Page *> &pages) {
  LinkResolver resolver{parser, {}, {}};
  for (std::size_t i = 0; i < pages.size(); ++i) {
    resolver.page_index.emplace(pages[i]->object_reference, i);
  }
  if (document.catalog != nullptr && document.catalog->object.is_dictionary()) {
    const pdf::Dictionary &catalog = document.catalog->object.as_dictionary();
    if (catalog.has_value("Dests")) {
      const pdf::Object dests =
          parser.resolve_object_copy(catalog.get("Dests"));
      if (dests.is_dictionary()) {
        for (const auto &[key, value] : dests.as_dictionary()) {
          resolver.named_dests.emplace(key, value);
        }
      }
    }
    if (catalog.has_value("Names")) {
      const pdf::Object names =
          parser.resolve_object_copy(catalog.get("Names"));
      if (names.is_dictionary() && names.as_dictionary().has_value("Dests")) {
        collect_dest_name_tree(parser, names.as_dictionary().get("Dests"),
                               resolver.named_dests, 0);
      }
    }
  }
  return resolver;
}

/// Whether a `/URI` action target is safe to emit as an `href`. A PDF is
/// untrusted input, so active schemes (`javascript:`, `data:`, `vbscript:`, …)
/// must not become a clickable link that executes in the generated document.
/// We allow only the common navigable schemes plus scheme-less (relative)
/// references. Embedded ASCII whitespace/control bytes are ignored when reading
/// the scheme, matching browsers that strip them before dispatch (so
/// `java\tscript:` cannot slip through).
bool is_safe_uri(std::string_view uri) {
  std::string scheme;
  for (const char ch : uri) {
    const auto c = static_cast<unsigned char>(ch);
    if (ch == ':') {
      for (char &s : scheme) {
        s = static_cast<char>(std::tolower(static_cast<unsigned char>(s)));
      }
      static constexpr std::string_view allowed[] = {"http", "https", "mailto",
                                                     "ftp",  "ftps",  "tel"};
      return std::find(std::begin(allowed), std::end(allowed), scheme) !=
             std::end(allowed);
    }
    if (ch == '/' || ch == '?' || ch == '#') {
      return true; // path/query/fragment reached first -> relative reference
    }
    if (c <= 0x20) {
      continue; // browsers strip embedded whitespace/control bytes
    }
    if (std::isalnum(c) != 0 || ch == '+' || ch == '-' || ch == '.') {
      scheme.push_back(ch);
      continue;
    }
    return true; // not a valid scheme character -> relative reference
  }
  return true; // no ':' -> relative reference
}

/// Resolve a page's `/Link` annotations (ISO 32000-1 12.5.6.5) to positioned
/// overlays: a `/URI` action becomes an external link, a `/GoTo` action or a
/// direct `/Dest` an internal link via `page_href`. `to_box` maps PDF user
/// space to the page box (points, y-down).
std::vector<LinkOut> collect_page_links(const pdf::Page &page,
                                        const util::math::Transform2D &to_box,
                                        LinkResolver &resolver,
                                        const PageHref &page_href) {
  std::vector<LinkOut> links;
  pdf::DocumentParser &parser = resolver.parser;
  for (const pdf::Annotation *annotation : page.annotations) {
    if (annotation == nullptr || !annotation->object.is_dictionary()) {
      continue;
    }
    const pdf::Dictionary &dictionary = annotation->object.as_dictionary();
    const pdf::Object subtype =
        parser.resolve_object_copy(dictionary.get("Subtype"));
    if (!subtype.is_name() || subtype.as_name() != "Link") {
      continue;
    }
    const pdf::Object rect = parser.resolve_object_copy(dictionary.get("Rect"));
    if (!rect.is_array() || rect.as_array().size() < 4) {
      continue;
    }
    const std::vector<double> r = rect.as_reals();

    std::string href;
    bool internal = false;
    if (dictionary.has_value("A")) {
      const pdf::Object action =
          parser.resolve_object_copy(dictionary.get("A"));
      if (action.is_dictionary()) {
        const pdf::Dictionary &a = action.as_dictionary();
        const pdf::Object s = parser.resolve_object_copy(a.get("S"));
        const std::string kind = s.is_name() ? s.as_name() : "";
        if (kind == "URI" && a.has_value("URI")) {
          const pdf::Object uri = parser.resolve_object_copy(a.get("URI"));
          if (uri.is_string() && is_safe_uri(uri.as_string())) {
            href = uri.as_string();
          }
        } else if (kind == "GoTo" && a.has_value("D")) {
          if (const auto index = resolver.resolve_dest_page(a.get("D"))) {
            href = page_href(*index);
            internal = true;
          }
        }
      }
    }
    if (href.empty() && dictionary.has_value("Dest")) {
      if (const auto index =
              resolver.resolve_dest_page(dictionary.get("Dest"))) {
        href = page_href(*index);
        internal = true;
      }
    }
    if (href.empty()) {
      continue;
    }

    const std::array<double, 2> p0 = to_box.apply(r[0], r[1]);
    const std::array<double, 2> p1 = to_box.apply(r[2], r[3]);
    LinkOut link;
    link.left = std::min(p0[0], p1[0]);
    link.top = std::min(p0[1], p1[1]);
    link.width = std::abs(p1[0] - p0[0]);
    link.height = std::abs(p1[1] - p0[1]);
    link.href = escape_attribute(std::move(href));
    link.internal = internal;
    links.push_back(std::move(link));
  }
  return links;
}

/// Write a page's link overlays as absolutely-positioned `<a>` elements (in
/// page-box points, matching the text layer's unit).
void write_page_links(HtmlWriter &out, const std::vector<LinkOut> &links) {
  for (const LinkOut &link : links) {
    std::ostringstream a;
    // Internal `#pN` links must override the document's `<base
    // target="_blank">` so they scroll within the rendered PDF instead of
    // opening a new copy.
    a << "<a class=\"lk\" href=\"" << link.href << '"'
      << (link.internal ? " target=\"_self\"" : "")
      << " style=\"left:" << round2(link.left) << "pt;top:" << round2(link.top)
      << "pt;width:" << round2(link.width)
      << "pt;height:" << round2(link.height) << "pt\"></a>";
    out.write_raw(std::move(a).str());
  }
}

/// Clamp a colour component in [0, 1] to an 8-bit channel value.
std::int32_t to255(const double v) {
  return static_cast<std::int32_t>(
      std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
}

/// Convert a PDF device color to a CSS `rgb(...)` string. Non-device color
/// spaces (Separation/ICCBased/…) are already converted to RGB at extract time;
/// only the unknown space reaches here, falling back to black (the PDF initial
/// color).
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
    // Naive CMYK -> RGB (no ICC).
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

/// Map a PDF blend-mode name (`/ExtGState` `/BM`, ISO 32000-1 11.3.5) to its
/// CSS `mix-blend-mode` keyword. CSS derives its blend modes from PDF, so the
/// separable and non-separable modes map 1:1 (camelCase -> kebab-case). Returns
/// "" for `Normal` and for any unrecognized name (rendered normal), so a caller
/// can skip the property entirely.
std::string blend_mode_to_css(const std::string &blend_mode) {
  static const std::unordered_map<std::string, std::string> map = {
      {"Multiply", "multiply"},     {"Screen", "screen"},
      {"Overlay", "overlay"},       {"Darken", "darken"},
      {"Lighten", "lighten"},       {"ColorDodge", "color-dodge"},
      {"ColorBurn", "color-burn"},  {"HardLight", "hard-light"},
      {"SoftLight", "soft-light"},  {"Difference", "difference"},
      {"Exclusion", "exclusion"},   {"Hue", "hue"},
      {"Saturation", "saturation"}, {"Color", "color"},
      {"Luminosity", "luminosity"}};
  const auto it = map.find(blend_mode);
  return it != map.end() ? it->second : std::string{};
}

/// The CSS declaration a non-embedded font renders through: its substitute
/// `font-family` stack plus the weight/style implied by the `/BaseFont` name
/// and `/FontDescriptor` flags. Interned as an `ff` atomic class on the
/// fallback (`font == 0`) runs of either text mode.
std::string font_substitute_declaration(const pdf::FontSubstitute &substitute) {
  std::string declaration = "font-family:" + substitute.css_family;
  if (substitute.bold) {
    declaration += ";font-weight:bold";
  }
  if (substitute.italic) {
    declaration += ";font-style:italic";
  }
  return declaration;
}

/// The `local(...)` sources of a CSS `font-family` stack, dropping the generic
/// keywords an `@font-face src` cannot name. Returns e.g.
/// "local('Times New Roman'),local(Times)" for "'Times New Roman',Times,serif",
/// or "" when the stack names no concrete font (generic-only).
std::string local_font_sources(const std::string_view css_family) {
  static constexpr std::array<std::string_view, 6> generics = {
      "serif", "sans-serif", "monospace", "cursive", "fantasy", "system-ui"};
  std::string src;
  std::size_t start = 0;
  while (start <= css_family.size()) {
    const std::size_t comma = css_family.find(',', start);
    std::string_view name = css_family.substr(
        start, comma == std::string_view::npos ? css_family.size() - start
                                               : comma - start);
    while (!name.empty() && name.front() == ' ') {
      name.remove_prefix(1);
    }
    while (!name.empty() && name.back() == ' ') {
      name.remove_suffix(1);
    }
    const bool generic =
        std::find(generics.begin(), generics.end(), name) != generics.end();
    if (!name.empty() && !generic) {
      if (!src.empty()) {
        src += ',';
      }
      src += "local(";
      src += name;
      src += ')';
    }
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return src;
}

/// Registers one `@font-face` per (substitute family, style, ascent) that
/// overrides the face's ascent/descent so a glyph's baseline lands exactly at
/// the `top` `add_position_classes` derives from `ascent_em` — independent of
/// the metrics of whichever local font actually resolves. Without the override
/// the browser positions the baseline using the resolved font's own ascent,
/// which for a large non-embedded run (e.g. a 120pt Times title) drops it well
/// below the intended baseline.
class SubstituteFontFaces {
public:
  /// The `font-family:...` (plus weight/style) declaration for `substitute`,
  /// routed through a generated metric-overriding face `'odr-sN'`. Falls back
  /// to the plain family stack when the stack names no concrete font.
  std::string declaration(const pdf::FontSubstitute &substitute,
                          const double ascent_em) {
    const std::string src = local_font_sources(substitute.css_family);
    if (src.empty()) {
      return font_substitute_declaration(substitute);
    }
    // ascent-override + descent-override sum to one em, so `line-height:1`
    // leaves no leading and the baseline sits at exactly `ascent_em` of the em
    // box. `ascent_em` is clamped to [0.5, 1.2]; the `max` keeps descent
    // non-negative for the rare ascent > 1 (a slight baseline approximation).
    const double ascent = ascent_em;
    const double descent = std::max(0.0, 1.0 - ascent_em);
    std::ostringstream key;
    key << src << '|' << substitute.bold << '|' << substitute.italic << '|'
        << std::llround(ascent * 1000.0);
    const auto [it, inserted] = m_index_by_key.try_emplace(
        std::move(key).str(), static_cast<int>(m_faces.size()) + 1);
    if (inserted) {
      std::ostringstream face;
      face << "@font-face{font-family:'odr-s" << it->second << "';src:" << src
           << ";ascent-override:" << round2(ascent * 100.0)
           << "%;descent-override:" << round2(descent * 100.0)
           << "%;line-gap-override:0%}";
      m_faces.push_back(std::move(face).str());
    }
    std::string declaration = "font-family:'odr-s" +
                              std::to_string(it->second) + "'," +
                              substitute.css_family;
    if (substitute.bold) {
      declaration += ";font-weight:bold";
    }
    if (substitute.italic) {
      declaration += ";font-style:italic";
    }
    return declaration;
  }

  /// Appends the collected `@font-face` rules to `out`.
  void append_faces(std::string &out) const {
    for (const std::string &face : m_faces) {
      out += face;
    }
  }

private:
  std::map<std::string, int> m_index_by_key;
  std::vector<std::string> m_faces;
};

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
    if (path.fill_alpha < 1) {
      f << " fill-opacity=\"" << round2(path.fill_alpha) << '"';
    }
  } else {
    f << " fill=\"none\"";
  }

  if (path.stroke) {
    f << " stroke=\"" << device_color_to_css(path.stroke_color) << '"';
    if (path.stroke_alpha < 1) {
      f << " stroke-opacity=\"" << round2(path.stroke_alpha) << '"';
    }
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

  if (const std::string blend = blend_mode_to_css(path.blend_mode);
      !blend.empty()) {
    f << " style=\"mix-blend-mode:" << blend << '"';
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
  if (image.alpha < 1) {
    f << " opacity=\"" << round2(image.alpha) << '"';
  }
  if (const std::string blend = blend_mode_to_css(image.blend_mode);
      !blend.empty()) {
    f << " style=\"mix-blend-mode:" << blend << '"';
  }
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
                                 const double height, const double alpha,
                                 const std::string &blend_mode) {
  if (gradient_id.empty()) {
    return {};
  }
  std::ostringstream f;
  f << "<rect x=\"0\" y=\"0\" width=\"" << round2(width) << "\" height=\""
    << round2(height) << "\" fill=\"url(#" << gradient_id << ")\"";
  if (!clip_id.empty()) {
    f << " clip-path=\"url(#" << clip_id << ")\"";
  }
  if (alpha < 1) {
    f << " opacity=\"" << round2(alpha) << '"';
  }
  if (const std::string blend = blend_mode_to_css(blend_mode); !blend.empty()) {
    f << " style=\"mix-blend-mode:" << blend << '"';
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

class MaskRegistry;

/// Serialize one graphic page element (a painted path, a shading flood, an
/// image, or a nested transparency group) to an SVG fragment in the page
/// viewBox, registering any clip, gradient, pattern or soft mask it needs.
/// Returns "" for a text element or one that paints nothing. A soft mask on the
/// element wraps its fragment in a masked `<g>`; a `GroupElement` renders its
/// children then wraps them in one `<g>` carrying the group's opacity, blend
/// and mask (so the group composites as a unit before those apply).
std::string render_graphic_fragment(const pdf::PageElement &element,
                                    const util::math::Transform2D &to_box,
                                    double width, double height,
                                    ClipRegistry &clips,
                                    GradientRegistry &gradients,
                                    PatternRegistry &patterns,
                                    MaskRegistry &masks, const Logger &logger);

/// Registers a page's soft masks (`/SMask`, ISO 32000-1 11.6.5.2) as `<mask>`
/// defs. The extractor has rendered each mask's transparency group into a list
/// of graphic elements (in user space); those are serialized into the mask body
/// with the page's own clip/gradient/pattern registries, so their ids stay
/// unique within the page. Coverage comes from luminance by default
/// (`/Luminosity` -> the SVG mask default) or from alpha (`/Alpha` ->
/// `mask-type="alpha"`); a non-black `/BC` backdrop floods behind the group.
/// Ids are namespaced per page (`m<page>_<n>`).
class MaskRegistry : public DefsRegistry {
public:
  using DefsRegistry::DefsRegistry;

  std::string register_mask(const pdf::SoftMask &mask,
                            const util::math::Transform2D &to_box,
                            const double width, const double height,
                            ClipRegistry &clips, GradientRegistry &gradients,
                            PatternRegistry &patterns, const Logger &logger) {
    // A fresh `SoftMask` is built for every `gs`, but many are identical (the
    // same drop-shadow reused across a run of glyphs, say). Dedupe on the
    // rendered body + type + backdrop so those collapse to a single def.
    std::ostringstream body;
    for (const pdf::PageElement &element : mask.group) {
      body << render_graphic_fragment(element, to_box, width, height, clips,
                                      gradients, patterns, *this, logger);
    }
    std::string signature = mask.type == pdf::SoftMask::Type::alpha ? "A" : "L";
    if (mask.backdrop.has_value()) {
      signature += rgb_to_css(*mask.backdrop);
    }
    signature += ';';
    signature += body.str();
    const auto [id, inserted] = intern(signature, "m");
    if (!inserted) {
      return id;
    }
    m_defs << "<mask id=\"" << id
           << "\" maskUnits=\"userSpaceOnUse\" x=\"0\" y=\"0\" width=\""
           << round2(width) << "\" height=\"" << round2(height) << '"';
    if (mask.type == pdf::SoftMask::Type::alpha) {
      m_defs << " mask-type=\"alpha\"";
    }
    m_defs << '>';
    // A non-black `/BC` backdrop floods the mask region behind the group; the
    // default (black) needs none — SVG's mask background is already luminance
    // 0.
    if (mask.backdrop.has_value() &&
        ((*mask.backdrop)[0] + (*mask.backdrop)[1] + (*mask.backdrop)[2] > 0)) {
      m_defs << "<rect x=\"0\" y=\"0\" width=\"" << round2(width)
             << "\" height=\"" << round2(height) << "\" fill=\""
             << rgb_to_css(*mask.backdrop) << "\"/>";
    }
    m_defs << std::move(body).str() << "</mask>";
    return id;
  }
};

/// Wrap an SVG fragment in a `<g>` carrying an opacity, a soft mask and/or a
/// blend mode, registering the mask. Returns `fragment` unchanged when none
/// apply (and "" for an empty fragment).
std::string wrap_effects(std::string fragment, const double alpha,
                         const std::shared_ptr<const pdf::SoftMask> &soft_mask,
                         const std::string &blend_mode,
                         const util::math::Transform2D &to_box,
                         const double width, const double height,
                         ClipRegistry &clips, GradientRegistry &gradients,
                         PatternRegistry &patterns, MaskRegistry &masks,
                         const Logger &logger) {
  if (fragment.empty()) {
    return fragment;
  }
  std::string mask_id;
  if (soft_mask != nullptr) {
    mask_id = masks.register_mask(*soft_mask, to_box, width, height, clips,
                                  gradients, patterns, logger);
  }
  const std::string blend = blend_mode_to_css(blend_mode);
  if (alpha >= 1 && mask_id.empty() && blend.empty()) {
    return fragment;
  }
  std::ostringstream g;
  g << "<g";
  if (alpha < 1) {
    g << " opacity=\"" << round2(alpha) << '"';
  }
  if (!mask_id.empty()) {
    g << " mask=\"url(#" << mask_id << ")\"";
  }
  if (!blend.empty()) {
    g << " style=\"mix-blend-mode:" << blend << '"';
  }
  g << '>' << fragment << "</g>";
  return std::move(g).str();
}

std::string render_graphic_fragment(const pdf::PageElement &element,
                                    const util::math::Transform2D &to_box,
                                    const double width, const double height,
                                    ClipRegistry &clips,
                                    GradientRegistry &gradients,
                                    PatternRegistry &patterns,
                                    MaskRegistry &masks, const Logger &logger) {
  const auto wrap_mask =
      [&](std::string fragment,
          const std::shared_ptr<const pdf::SoftMask> &soft_mask) {
        return wrap_effects(std::move(fragment), 1.0, soft_mask, "", to_box,
                            width, height, clips, gradients, patterns, masks,
                            logger);
      };
  if (const auto *path = std::get_if<pdf::PathElement>(&element)) {
    const std::string clip_id = clips.register_clip(path->clip, to_box);
    std::string fill_url_id;
    if (path->fill_shading != nullptr) {
      fill_url_id = gradients.register_gradient(
          *path->fill_shading, path->shading_transform * to_box);
    } else if (path->fill_pattern != nullptr) {
      fill_url_id = patterns.register_pattern(*path->fill_pattern,
                                              path->pattern_transform * to_box,
                                              path->fill_color, logger);
    }
    return wrap_mask(svg_path_fragment(*path, to_box, clip_id, fill_url_id),
                     path->soft_mask);
  }
  if (const auto *shading = std::get_if<pdf::ShadingElement>(&element)) {
    if (shading->shading == nullptr) {
      return {};
    }
    const std::string clip_id = clips.register_clip(shading->clip, to_box);
    const std::string gradient_id = gradients.register_gradient(
        *shading->shading, shading->transform * to_box);
    return wrap_mask(svg_shading_fragment(gradient_id, clip_id, width, height,
                                          shading->alpha, shading->blend_mode),
                     shading->soft_mask);
  }
  if (const auto *image = std::get_if<pdf::ImageElement>(&element)) {
    const std::string clip_id = clips.register_clip(image->clip, to_box);
    return wrap_mask(svg_image_fragment(*image, to_box, clip_id),
                     image->soft_mask);
  }
  if (const auto *group = std::get_if<pdf::GroupElement>(&element)) {
    if (group->children == nullptr) {
      return {};
    }
    std::string inner;
    for (const pdf::PageElement &child : group->children->elements) {
      inner += render_graphic_fragment(child, to_box, width, height, clips,
                                       gradients, patterns, masks, logger);
    }
    return wrap_effects(std::move(inner), group->alpha, group->soft_mask,
                        group->blend_mode, to_box, width, height, clips,
                        gradients, patterns, masks, logger);
  }
  return {};
}

/// Lifts text out of transparency groups so this HTML backend can still render
/// and select it. A `GroupElement`'s opacity/blend/mask is carried by an SVG
/// `<g>`, but text is painted as positioned markup, not SVG, so it cannot ride
/// inside that `<g>`. The extractor faithfully nests such text in the group; to
/// avoid dropping it from both the visual and selection layers, each interior
/// `TextElement` is hoisted to the top level — where the ordinary text pipeline
/// renders it and `extract_text`-style top-level scans pick it up. The only
/// thing forgone is the group effect on the text itself (see pdf/AGENTS.md
/// gaps); the group's graphics are untouched and still composited as a unit.
/// Nesting is flattened recursively; hoisted text is emitted at the group's
/// position (ahead of the composited graphics), and a group left with no
/// graphics is dropped.
std::vector<pdf::PageElement>
lift_group_text(std::vector<pdf::PageElement> elements) {
  std::vector<pdf::PageElement> result;
  result.reserve(elements.size());
  for (pdf::PageElement &element : elements) {
    auto *group = std::get_if<pdf::GroupElement>(&element);
    if (group == nullptr || group->children == nullptr) {
      result.push_back(std::move(element));
      continue;
    }
    // Flatten nested groups first, so all interior text sits at this level.
    std::vector<pdf::PageElement> inner =
        lift_group_text(group->children->elements);
    std::vector<pdf::PageElement> graphics;
    graphics.reserve(inner.size());
    for (pdf::PageElement &child : inner) {
      if (std::holds_alternative<pdf::TextElement>(child)) {
        result.push_back(std::move(child)); // hoisted ahead of the group
      } else {
        graphics.push_back(std::move(child));
      }
    }
    if (graphics.empty()) {
      continue; // the group carried only text; nothing left to composite
    }
    auto children = std::make_shared<pdf::GroupChildren>();
    children->elements = std::move(graphics);
    pdf::GroupElement lifted;
    lifted.children = std::move(children);
    lifted.alpha = group->alpha;
    lifted.blend_mode = group->blend_mode;
    lifted.soft_mask = group->soft_mask;
    result.push_back(std::move(lifted));
  }
  return result;
}

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
        m_pdf_file{std::move(pdf_file)} {}

  /// Parses the document once, applies the `[page_range_begin,
  /// page_range_end)` page range and builds the view list: the combined
  /// document plus one standalone view per rendered page. The parser and its
  /// object cache are kept for the service's lifetime so the page views
  /// render off one shared parse.
  void warmup() const override {
    std::lock_guard lock(m_mutex);

    if (m_document != nullptr) {
      return;
    }

    const auto &pdf_file =
        dynamic_cast<const pdf::PdfFile &>(*m_pdf_file.impl());
    m_parser = pdf_file.create_parser(*m_logger);
    m_document = m_parser->parse_document();

    const std::vector<pdf::Page *> pages = m_document->collect_pages();
    m_link_resolver = std::make_unique<LinkResolver>(
        build_link_resolver(*m_parser, *m_document, pages));
    const std::size_t begin =
        std::min<std::size_t>(config().page_range_begin, pages.size());
    const std::size_t end =
        config().page_range_end
            ? std::clamp<std::size_t>(*config().page_range_end, begin,
                                      pages.size())
            : pages.size();
    m_first_page = begin;
    m_pages.assign(pages.begin() + static_cast<std::ptrdiff_t>(begin),
                   pages.begin() + static_cast<std::ptrdiff_t>(end));

    m_views.emplace_back(std::make_shared<HtmlView>(
        *this, "document", 0, config().document_output_file_name));
    for (std::size_t i = 0; i < m_pages.size(); ++i) {
      const std::size_t page = m_first_page + i;
      m_views.emplace_back(std::make_shared<HtmlView>(
          *this, "page" + std::to_string(page + 1), page + 1,
          fill_path_variables(config().page_output_file_name, page)));
    }
  }

  [[nodiscard]] const HtmlViews &list_views() const override {
    warmup();
    return m_views;
  }

  [[nodiscard]] bool exists(const std::string &path) const override {
    warmup();
    return std::ranges::any_of(
        m_views, [&path](const auto &view) { return view.path() == path; });
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (exists(path)) {
      return "text/html";
    }
    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    HtmlWriter writer(out, config());
    write_html(path, writer);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    warmup();
    // The parser's object cache is shared mutable state; renders are
    // serialized.
    std::lock_guard lock(m_mutex);
    if (path == config().document_output_file_name) {
      return write_document(out);
    }
    for (std::size_t i = 0; i < m_pages.size(); ++i) {
      if (path == m_views[i + 1].path()) {
        return write_page(i, out);
      }
    }
    throw FileNotFound("Unknown path: " + path);
  }

  /// Whether the 0-based page index falls inside the rendered page range.
  [[nodiscard]] bool page_rendered(const std::size_t index) const {
    return index >= m_first_page && index < m_first_page + m_pages.size();
  }

  /// The combined document: every rendered page, internal links as `#pN`
  /// anchors (dropped when the target page is outside the page range).
  HtmlResources write_document(HtmlWriter &out) const {
    const PageHref page_href = [this](const std::size_t index) {
      return page_rendered(index) ? "#p" + std::to_string(index + 1)
                                  : std::string();
    };
    return write_pages(out, m_pages, m_first_page + 1, page_href);
  }

  /// One standalone page (the `page{index}.html` view); internal links
  /// navigate between the page files, rebased onto this page's own directory
  /// (the browser resolves an href against the current document, so a nested
  /// `page_output_file_name` must not be emitted output-root-relative).
  HtmlResources write_page(const std::size_t page_index,
                           HtmlWriter &out) const {
    const RelPath from_dir = RelPath(m_views[page_index + 1].path()).parent();
    const PageHref page_href = [this, &from_dir](const std::size_t index) {
      return page_rendered(index)
                 ? RelPath(fill_path_variables(config().page_output_file_name,
                                               index))
                       .rebase(from_dir)
                       .string()
                 : std::string();
    };
    const std::vector<pdf::Page *> pages{m_pages[page_index]};
    return write_pages(out, pages, m_first_page + page_index + 1, page_href);
  }

  HtmlResources write_pages(HtmlWriter &out,
                            const std::vector<pdf::Page *> &pages,
                            const std::size_t first_page_number,
                            const PageHref &page_href) const {
    if (config().pdf_text_mode == PdfTextMode::single_layer) {
      return write_pages_single_layer(out, pages, first_page_number, page_href);
    }
    return write_pages_dual_layer(out, pages, first_page_number, page_href);
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
    std::vector<LinkOut> links;
  };

  HtmlResources write_pages_dual_layer(HtmlWriter &out,
                                       const std::vector<pdf::Page *> &pages,
                                       const std::size_t first_page_number,
                                       const PageHref &page_href) const {
    HtmlResources resources;

    pdf::DocumentParser &parser = *m_parser;
    LinkResolver &link_resolver = *m_link_resolver;

    AtomicStyles styles;
    std::vector<DualPageOut> pages_out;
    pages_out.reserve(pages.size());

    // Font management — visual layer only needs PUA glyphs.
    std::uint32_t family_count = 0;
    std::string font_faces;
    std::string font_styles; // per-font `.fvN` (visible) / `.fnN` (invisible)
    SubstituteFontFaces substitute_faces; // metric-overriding `.odr-sN` faces
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
      page_out.links =
          collect_page_links(*page, to_box, link_resolver, page_href);

      std::string stream;
      for (const auto &ref : page->contents_reference) {
        stream += parser.read_decoded_stream(ref);
        stream += '\n';
      }

      ClipRegistry clips(static_cast<std::uint32_t>(pages_out.size()));
      GradientRegistry gradients(static_cast<std::uint32_t>(pages_out.size()));
      PatternRegistry patterns(static_cast<std::uint32_t>(pages_out.size()));
      MaskRegistry masks(static_cast<std::uint32_t>(pages_out.size()));

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

      for (const pdf::PageElement &element : lift_group_text(
               pdf::extract_page(stream, *page->resources, *m_logger))) {
        if (handle_graphic_element(
                element, to_box, width, height, clips, gradients, patterns,
                masks, *m_logger, [&] { vis_close_line(); },
                [&](std::string frag) {
                  page_out.vis_items.push_back(PathOut{std::move(frag)});
                })) {
          continue;
        }

        const pdf::TextElement &text = std::get<pdf::TextElement>(element);
        // TODO(clip text): clip not applied to text (see pdf/AGENTS.md gaps).
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
        // layer. Type3 runs are painted by their char procs (separate path/
        // image elements), so they too contribute only to the selection layer.
        if (!invisible && !text.render_as_graphics) {
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
          } else if (text.font != nullptr && text.font->substitute) {
            // Non-embedded font: render the real Unicode in the substitute
            // family (embedded fonts carry the family in `font_class`). The
            // metric-overriding face pins the baseline to `asc` (see
            // `SubstituteFontFaces`).
            add_class(
                run_classes, "ff",
                substitute_faces.declaration(*text.font->substitute, asc));
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
      page_out.clip_defs =
          clips.defs() + gradients.defs() + patterns.defs() + masks.defs();
    }

    // Post-pass: re-encode accepted fonts PUA-only.
    for (std::uint32_t i = 0; i < family_count; ++i) {
      write_font_face(*accepted_fonts[i], i, {}, font_class_used[i], font_faces,
                      font_styles);
    }
    substitute_faces.append_faces(font_faces);

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
    std::size_t page_number = first_page_number;
    for (const DualPageOut &page : pages_out) {
      out.write_element_begin(
          "div",
          HtmlElementOptions()
              .set_class(page.classes)
              .set_extra(R"(id="p)" + std::to_string(page_number++) + R"(")"));

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

      // Link overlays on top so they stay clickable.
      write_page_links(out, page.links);

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
    bool lead_space{false}; ///< clean run: emit a selectable space before
                            ///< `text` (a recovered inferred word break)
    std::string lead_space_width; ///< "" (zero-width `.ov`) or a width class:
                                  ///< the pdf2htmlEX-style width-bearing space
                                  ///< that also carries the gap (no run margin)
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
    std::vector<LinkOut> links;
  };

  HtmlResources write_pages_single_layer(HtmlWriter &out,
                                         const std::vector<pdf::Page *> &pages,
                                         const std::size_t first_page_number,
                                         const PageHref &page_href) const {
    HtmlResources resources;

    pdf::DocumentParser &parser = *m_parser;
    LinkResolver &link_resolver = *m_link_resolver;

    // ---- Font registration ------------------------------------------------
    // A real-Unicode scalar gets a cmap entry only inside the BMP and outside
    // the PUA (`U+E000..U+F8FF`), so glyph-deterministic PUA code points are
    // never shadowed.
    const auto collapsible_unicode = [](const char32_t c) {
      return c <= 0xFFFF && !(c >= 0xE000 && c <= 0xF8FF);
    };

    // A leading inferred space (space inference) carries no character code or
    // advance, so the "collapsible" 1:1 alignment is between the codes and the
    // run text *after* that space. These helpers view the run text past it: the
    // core character count (to test 1:1 against `advances`) and the byte offset
    // where the codes begin (to walk `text` alongside `font->codes`).
    const auto core_char_count = [](const pdf::TextElement &t) {
      return util::string::utf8_length(t.text) -
             (t.leading_space_inferred ? 1u : 0u);
    };
    const auto core_text_begin = [](const pdf::TextElement &t) {
      auto cp = t.text.begin();
      if (t.leading_space_inferred) {
        utf8::unchecked::next(cp); // skip the one-byte U+0020
      }
      return cp;
    };

    std::uint32_t family_count = 0;
    std::string font_faces;
    std::string font_styles;              // ".fvN{...}" / ".fnN{...}"
    SubstituteFontFaces substitute_faces; // metric-overriding `.odr-sN` faces
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
      for (const pdf::PageElement &element : lift_group_text(pdf::extract_page(
               page_streams[pi], *page.resources, *m_logger))) {
        const auto *text = std::get_if<pdf::TextElement>(&element);
        if (text == nullptr || text->text.empty() || text->font == nullptr) {
          continue;
        }
        const std::uint32_t font = font_family(text->font);
        if (font == 0) {
          continue;
        }
        // Only collapsible-candidate runs contribute to frequency counts. A
        // leading inferred space is skipped so a run carrying one still votes.
        if (core_char_count(*text) != text->advances.size()) {
          continue;
        }
        auto cp = core_text_begin(*text);
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
      page_out.links =
          collect_page_links(page, to_box, link_resolver, page_href);

      ClipRegistry clips(static_cast<std::uint32_t>(pages_out.size()));
      GradientRegistry gradients(static_cast<std::uint32_t>(pages_out.size()));
      PatternRegistry patterns(static_cast<std::uint32_t>(pages_out.size()));
      MaskRegistry masks(static_cast<std::uint32_t>(pages_out.size()));

      std::int32_t cur_line = -1;
      std::string cur_flow_key;
      bool prev_was_matrix = false;
      double prev_end = 0;
      double prev_baseline = 0;
      double prev_font_pt = 0;
      const auto close_line = [&] { cur_line = -1; };

      for (const pdf::PageElement &element : lift_group_text(pdf::extract_page(
               page_streams[pi], *page.resources, *m_logger))) {
        if (handle_graphic_element(
                element, to_box, width, height, clips, gradients, patterns,
                masks, *m_logger, [&] { close_line(); },
                [&](std::string frag) {
                  page_out.items.push_back(SinglePathOut{std::move(frag)});
                })) {
          continue;
        }

        const pdf::TextElement &text = std::get<pdf::TextElement>(element);
        // TODO(clip text): clip not applied to text (see pdf/AGENTS.md gaps).
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
          // Check collapse: 1:1 codes↔core-text and every (uchar, glyph)
          // matches the frequency winner. A leading inferred space is metadata,
          // not a coded char, so it is excluded from the alignment (otherwise a
          // recovered word break would force the whole run onto the PUA path).
          // `font != 0` here already implies `text.font != nullptr`.
          bool collapse = core_char_count(text) > 0 &&
                          core_char_count(text) == text.advances.size();
          if (collapse) {
            const std::map<char32_t, std::uint16_t> &won =
                used_unicode[font - 1];
            auto cp = core_text_begin(text);
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
            // Real Unicode renders directly via the cmap. Any leading inferred
            // space becomes a dedicated selectable space span (`lead_space`),
            // never a literal space in `text`, so `white-space:pre` cannot
            // shift the glyphs right of their placement origin. When the run
            // continues its line over a positive gap the span carries that gap
            // as its width (pdf2htmlEX's model: one real space that is both the
            // copyable character and the visible advance); otherwise it stays
            // zero-width. See the `.sp`/`.ov` split in the main pass.
            run.lead_space = text.leading_space_inferred;
            run.text = escape_markup(
                std::string(core_text_begin(text), text.text.end()));
          } else {
            run.glyph_data = glyph_run_str(*text.font, text.codes);
            run.text = escape_markup(text.text); // overlay (empty=no_unicode)
          }
        }
        if (run.text.empty() && run.glyph_data.empty()) {
          continue; // invisible no_unicode
        }

        // ---- Flow grouping -----------------------------------------------
        // The visible substitute family of a non-embedded font (`font == 0`);
        // part of the flow key so two different substitutes (e.g. a Helvetica
        // run then a Times run) never share one line block's `font_class`.
        const std::string substitute_declaration =
            (font == 0 && !invisible && text.font != nullptr &&
             text.font->substitute)
                ? substitute_faces.declaration(*text.font->substitute, asc)
                : std::string();
        std::ostringstream fk;
        fk << font << '|' << invisible << '|' << font_size_pt << '|' << cs_pt
           << '|' << ws_pt << '|' << substitute_declaration;
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
          // Invisible (Tr 3/7) and Type3 (painted by char procs) runs stay in
          // the DOM for selection but render transparent.
          if (font == 0 && (invisible || text.render_as_graphics)) {
            base += " i";
          }

          SingleLineOut line;
          line.classes = std::move(base);
          if (font != 0) {
            line.font_class = font_class(font_class_used, font, invisible);
          } else if (!substitute_declaration.empty()) {
            line.font_class = styles.intern("ff", substitute_declaration);
          }
          line.runs.push_back(std::move(run));
          page_out.items.push_back(std::move(line));
          cur_line = static_cast<int>(page_out.items.size()) - 1;
          cur_flow_key = flow_key;
        } else {
          if (run.lead_space && margin_pt > 0) {
            // Recovered word break over a positive gap: fold the advance into
            // the selectable space span (pdf2htmlEX-style width-bearing space)
            // rather than a zero-width `.ov` plus a `margin-left` on the run.
            run.lead_space_width =
                styles.intern("w", pt_decl("width", margin_pt));
          } else if (margin_pt != 0) {
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

      page_out.clip_defs =
          clips.defs() + gradients.defs() + patterns.defs() + masks.defs();
    }

    // ---- Post-pass: re-encode fonts with frequency-winner cmap entries ---
    for (std::uint32_t i = 0; i < family_count; ++i) {
      write_font_face(*accepted_fonts[i], i, used_unicode[i],
                      font_class_used[i], font_faces, font_styles);
    }
    substitute_faces.append_faces(font_faces);

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
      // Width-bearing selectable space for a recovered word break (pdf2htmlEX's
      // model): a real `" "` inside an inline-block sized to the gap by a `wN`
      // class, so the space is markable/copyable *and* carries the advance
      // (no `margin-left` on the following run). No `overflow:hidden`: it would
      // not clip the space glyph (transparent, and the declared width already
      // fixes the advance) but *would* move the inline-block's baseline to its
      // bottom margin edge (CSS quirk when overflow != visible), lifting the
      // space's selection box off the text baseline so it highlights at the
      // wrong height. `vertical-align:baseline` on a `visible` box keeps the
      // space aligned with the neighbouring glyphs.
      out.out() << ".sp{display:inline-block;"
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
          if (run.lead_space) {
            // Recovered word-break space: a real, selectable/copyable `" "`.
            // With a `wN` width it carries the gap (pdf2htmlEX-style,
            // markable); without one it is zero-width `.ov` (line-leading or
            // negative gap) and never shifts the glyphs under
            // `white-space:pre`.
            const std::string space_cls = run.lead_space_width.empty()
                                              ? std::string("ov")
                                              : "sp " + run.lead_space_width;
            out.write_element_begin(
                "span",
                HtmlElementOptions().set_inline(true).set_class(space_cls));
            out.write_raw(" ");
            out.write_element_end("span");
          }
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
    std::size_t page_number = first_page_number;
    for (const SinglePageOut &page : pages_out) {
      out.write_element_begin(
          "div",
          HtmlElementOptions()
              .set_class(page.classes)
              .set_extra(R"(id="p)" + std::to_string(page_number++) + R"(")"));
      write_page_items(out, page.clip_defs, page.items, page.width, page.height,
                       write_line);
      write_page_links(out, page.links);
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
    const bool stroked =
        text.rendering_mode == pdf::TextRenderingMode::stroke ||
        text.rendering_mode == pdf::TextRenderingMode::stroke_clip;
    const bool fill_and_stroke =
        text.rendering_mode == pdf::TextRenderingMode::fill_stroke ||
        text.rendering_mode == pdf::TextRenderingMode::fill_stroke_clip;
    const pdf::GraphicsState::Color &paint =
        stroked ? text.stroke_color : text.fill_color;
    // The single span carries one opacity. A fill-and-stroke glyph paints both,
    // so take the more opaque of the two: a transparent fill (`ca 0`) must not
    // hide an opaque stroke (`CA 1`) and drop the outlined glyph entirely.
    const double alpha = stroked ? text.stroke_alpha
                         : fill_and_stroke
                             ? std::max(text.fill_alpha, text.stroke_alpha)
                             : text.fill_alpha;
    const std::string blend = blend_mode_to_css(text.blend_mode);
    std::string css = device_color_to_css(paint);
    // Fast path: default black, fully opaque, no blend — no class needed.
    if (css == "rgb(0,0,0)" && alpha >= 1 && blend.empty()) {
      return {};
    }
    std::ostringstream declaration;
    declaration << "color:" << css;
    if (alpha < 1) {
      declaration << ";opacity:" << round2(alpha);
    }
    if (!blend.empty()) {
      declaration << ";mix-blend-mode:" << blend;
    }
    return ' ' + styles.intern("k", std::move(declaration).str());
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
    // Link annotation overlays (absolutely positioned in page-box points).
    out.out() << ".lk{position:absolute;transform-origin:0 0}";
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
        (void)sfnt->write();
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
      reencoded = sfnt->write();
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
  static bool
  handle_graphic_element(const pdf::PageElement &element,
                         const util::math::Transform2D &to_box, double width,
                         double height, ClipRegistry &clips,
                         GradientRegistry &gradients, PatternRegistry &patterns,
                         MaskRegistry &masks, Logger &logger,
                         CloseLine &&close_line, PushSvg &&push_svg) {
    // Text is handled by the caller; every other element kind is a graphic.
    if (std::holds_alternative<pdf::TextElement>(element)) {
      return false;
    }
    std::string frag =
        render_graphic_fragment(element, to_box, width, height, clips,
                                gradients, patterns, masks, logger);
    if (!frag.empty()) {
      close_line();
      push_svg(std::move(frag));
    }
    return true;
  }

protected:
  PdfFile m_pdf_file;

  // Lazily initialized by `warmup()` (all guarded by `m_mutex`): one parse
  // shared by the combined-document and per-page renders.
  mutable std::mutex m_mutex;
  mutable std::unique_ptr<pdf::DocumentParser> m_parser;
  mutable std::unique_ptr<pdf::Document> m_document;
  mutable std::unique_ptr<LinkResolver> m_link_resolver;
  /// The rendered pages (`[page_range_begin, page_range_end)`) and the 0-based
  /// document-global index of the first one.
  mutable std::vector<pdf::Page *> m_pages;
  mutable std::size_t m_first_page{0};
  mutable HtmlViews m_views;
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
