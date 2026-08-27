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
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf/pdf_color.hpp>
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
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odr::internal::html {

namespace {

/// Round to 0.01 units; finer precision is invisible and the extra digits add
/// up across a page full of path data.
double round2(const double v) { return std::round(v * 100.0) / 100.0; }

constexpr double pt_to_in = 1.0 / 72.0;

/// A transform as an SVG `matrix(...)`. Only the translation is rounded; the
/// linear part keeps full precision so small scale/skew factors aren't
/// quantized to zero.
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

/// A link's 0-based target page index to the href navigating to it. Returns ""
/// to drop the link (target page not rendered).
using PageHref = std::function<std::string(std::size_t)>;

/// Resolves a link annotation's destination to a page index, via the catalog's
/// named-destination tables (ISO 32000-1 12.3.2.3).
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
                                 const std::span<pdf::Page *const> pages) {
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

/// Whether a `/URI` action target is safe to emit as an `href`: only the
/// navigable schemes plus scheme-less (relative) references, so `javascript:`
/// and friends cannot become a clickable link. Embedded whitespace/control
/// bytes are ignored while reading the scheme, as browsers strip them before
/// dispatch (`java\tscript:` must not slip through).
bool is_safe_uri(std::string_view uri) {
  std::string scheme;
  for (const char ch : uri) {
    const auto c = static_cast<unsigned char>(ch);
    if (ch == ':') {
      static constexpr std::array<std::string_view, 6> allowed = {
          "http", "https", "mailto", "ftp", "ftps", "tel"};
      return std::ranges::any_of(allowed, [&scheme](const std::string_view s) {
        return util::string::equals_ignore_case(scheme, s);
      });
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

/// A page's `/Link` annotations (ISO 32000-1 12.5.6.5) as positioned overlays.
/// `to_box` maps PDF user space to the page box (points, y-down).
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
    // target="_blank">` or they open a new copy instead of scrolling.
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

/// A PDF device color as CSS `rgb(...)`. Other spaces are already converted at
/// extract time; only `unknown` reaches here, falling back to black.
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
    const std::array<double, 3> rgb = pdf::cmyk_to_rgb(
        color.cmyk[0], color.cmyk[1], color.cmyk[2], color.cmyk[3]);
    r = to255(rgb[0]);
    g = to255(rgb[1]);
    b = to255(rgb[2]);
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

/// A PDF blend-mode name (`/BM`, ISO 32000-1 11.3.5) as its CSS
/// `mix-blend-mode` keyword — CSS took its blend modes from PDF, so they map
/// 1:1. "" for `Normal` and for anything unrecognized, so callers can skip the
/// property entirely.
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
/// family stack plus the weight/style implied by `/BaseFont` and the
/// `/FontDescriptor` flags.
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

/// The suffixes naming a family's styled cut, most specific first. `local()`
/// matches a face name, not a family plus a weight, so bold must be asked for.
std::vector<std::string_view> style_suffixes(const bool bold,
                                             const bool italic) {
  if (bold && italic) {
    return {"-BoldItalic", " Bold Italic", "-BoldOblique", " Bold Oblique"};
  }
  if (bold) {
    return {"-Bold", " Bold"};
  }
  if (italic) {
    return {"-Italic", " Italic", "-Oblique", " Oblique"};
  }
  return {};
}

/// The `local(...)` sources of a `font-family` stack, dropping the generic
/// keywords an `@font-face src` cannot name, each family styled-cut-first so a
/// system without the cut still resolves. "" when the stack is generic-only.
std::string local_font_sources(const std::string_view css_family,
                               const bool bold, const bool italic) {
  static constexpr std::array<std::string_view, 6> generics = {
      "serif", "sans-serif", "monospace", "cursive", "fantasy", "system-ui"};
  const std::vector<std::string_view> suffixes = style_suffixes(bold, italic);
  std::string src;
  const auto add = [&src](const std::string_view name,
                          const std::string_view suffix) {
    if (!src.empty()) {
      src += ',';
    }
    src += "local(\"";
    src += name;
    src += suffix;
    src += "\")";
  };
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
    // The stack quotes a name that needs it ('Times New Roman'); `add` quotes
    // again, and a doubly quoted name matches no installed face.
    if (name.size() > 1 && (name.front() == '\'' || name.front() == '"') &&
        name.back() == name.front()) {
      name.remove_prefix(1);
      name.remove_suffix(1);
    }
    const bool generic = std::ranges::find(generics, name) != generics.end();
    if (!name.empty() && !generic) {
      for (const std::string_view suffix : suffixes) {
        add(name, suffix);
      }
      add(name, "");
    }
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return src;
}

/// One `@font-face` per (substitute family, style, ascent) overriding the
/// face's ascent/descent, so a glyph's baseline lands at the `top`
/// `add_position_classes` derived from `ascent_em` rather than wherever
/// whichever local font resolves would put it.
class SubstituteFontFaces {
public:
  /// The `font-family:...` (plus weight/style) declaration for `substitute`,
  /// routed through a generated metric-overriding face `'odr-sN'`. Falls back
  /// to the plain family stack when the stack names no concrete font.
  std::string declaration(const pdf::FontSubstitute &substitute,
                          const double ascent_em) {
    const std::string src = local_font_sources(
        substitute.css_family, substitute.bold, substitute.italic);
    if (src.empty()) {
      return font_substitute_declaration(substitute);
    }
    // The two overrides sum to one em, so `line-height:1` leaves no leading and
    // the baseline sits at exactly `ascent_em`. `max` keeps descent
    // non-negative for the rare clamped ascent > 1.
    const double ascent = ascent_em;
    const double descent = std::max(0.0, 1.0 - ascent_em);
    std::ostringstream key;
    key << src << '|' << substitute.bold << '|' << substitute.italic << '|'
        << std::llround(ascent * 1000.0);
    const auto [it, inserted] = m_index_by_key.try_emplace(
        std::move(key).str(), static_cast<int>(m_faces.size()) + 1);
    if (inserted) {
      std::ostringstream face;
      face << "@font-face{font-family:'odr-s" << it->second << "';src:" << src;
      // Declared, so the browser does not synthesise them a second time.
      if (substitute.bold) {
        face << ";font-weight:bold";
      }
      if (substitute.italic) {
        face << ";font-style:italic";
      }
      face << ";ascent-override:" << round2(ascent * 100.0)
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

/// An SVG `d` attribute for a path's subpaths, each point mapped through
/// `to_box` (PDF user space -> the page box, y-down).
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

/// A painted path as an SVG `<path .../>` fragment in the page viewBox, or ""
/// when it paints nothing. `clip_id` and `fill_url_id`, when non-empty, name a
/// `<clipPath>` and a paint server (gradient or tiling pattern) to reference.
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
    // A 0 width is "device-thinnest" in PDF; SVG would draw nothing.
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

/// An image XObject as an SVG `<image>` fragment in the page viewBox, or ""
/// when it carries no pass-through bytes. The image fills the unit square in
/// user space (ISO 32000-1 8.10.5), flipped vertically because its first row is
/// its top. `clip_id` is installed on a wrapping `<g>`, not on the `<image>`:
/// the clip geometry is `userSpaceOnUse` in the viewBox, and on the image it
/// would resolve in the image's post-transform unit-square space instead.
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

/// Shared bookkeeping for the per-page `<defs>` registries below: a
/// signature->id cache deduplicating repeated definitions plus the accumulated
/// `<defs>` markup. Ids are namespaced per page as `<prefix><page>_<n>`.
class DefsRegistry {
public:
  explicit DefsRegistry(const std::uint32_t page) : m_page{page} {}

  [[nodiscard]] std::string defs() const { return m_defs.str(); }

protected:
  /// The id for `signature`. `inserted` is true only on first sight, when the
  /// caller still has to emit the definition into `m_defs`.
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

/// A page's clip regions as nested `<clipPath>` defs (`c<page>_<n>`). PDF's
/// current clip is the *intersection* of an ordered region list; SVG expresses
/// intersection by chaining `clip-path`, so region i references region i-1 and
/// the painted element references the last.
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

/// A page's axial/radial shadings as `<linearGradient>`/`<radialGradient>` defs
/// (`g<page>_<n>`), placed by `gradientTransform` in `userSpaceOnUse`.
///
/// DEFERRED: `/Extend` is approximated by SVG's default `pad` spread, so a
/// non-extended shading over-paints beyond its interval; `Shading::background`
/// and `Shading::bbox` are not honoured. Both need the fill clipped to the
/// gradient band/annulus.
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

/// An `sh` shading flood as an SVG `<rect>` spanning the page box, filled with
/// `gradient_id`; `clip_id` (and the gradient's own extent) bound the paint.
/// "" when the shading produced no gradient.
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

/// A page's tiling patterns (`/PatternType 1`) as SVG `<pattern>` defs
/// (`pat<page>_<n>`). The content stream is run as a mini page into tile
/// fragments in pattern space, repeated every `/XStep`/`/YStep` and placed by
/// `patternTransform`. An uncoloured pattern (`/PaintType 2`) paints in the
/// path's fill colour, so the cache key folds that colour in. Only paths and
/// images are rendered (nested text/shadings/patterns are skipped — rare).
/// "" for an unrepresentable pattern.
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
    // lattice (step < BBox) has no single-`<pattern>` equivalent and is lost.
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

/// One graphic page element as an SVG fragment in the page viewBox,
/// registering any clip, gradient, pattern or soft mask it needs. "" for a text
/// element or one that paints nothing. A `GroupElement`'s children are wrapped
/// in a single `<g>` so the group composites as a unit before its
/// opacity/blend/mask apply.
std::string render_graphic_fragment(const pdf::PageElement &element,
                                    const util::math::Transform2D &to_box,
                                    double width, double height,
                                    ClipRegistry &clips,
                                    GradientRegistry &gradients,
                                    PatternRegistry &patterns,
                                    MaskRegistry &masks, const Logger &logger);

/// A page's soft masks (`/SMask`, ISO 32000-1 11.6.5.2) as `<mask>` defs
/// (`m<page>_<n>`). The extractor has already rendered each mask's transparency
/// group to graphic elements; those are serialized through the page's own
/// clip/gradient/pattern registries so their ids stay unique within the page.
class MaskRegistry : public DefsRegistry {
public:
  using DefsRegistry::DefsRegistry;

  std::string register_mask(const pdf::SoftMask &mask,
                            const util::math::Transform2D &to_box,
                            const double width, const double height,
                            ClipRegistry &clips, GradientRegistry &gradients,
                            PatternRegistry &patterns, const Logger &logger) {
    // A fresh `SoftMask` is built for every `gs`, but many are identical (one
    // drop-shadow across a run of glyphs); dedupe on the rendered body.
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
    // Black — SVG's mask background already — needs no backdrop rect.
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

/// Hoists text out of transparency groups (recursively) to the top level. A
/// group's effects ride an SVG `<g>`, but text is positioned markup, not SVG,
/// so it cannot sit inside that `<g>` — without the hoist it would be dropped
/// from both the visual and the selection layer. Forgone: the group effect on
/// the text itself (see pdf/AGENTS.md gaps). The group's graphics are untouched
/// and still composite as a unit; a group left with none is dropped.
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

/// A page's content stream, then the annotation appearances painting on top of
/// it (ISO 32000-1 12.5.5).
std::vector<pdf::PageElement> page_elements(const pdf::Page &page,
                                            const std::string &content,
                                            const Logger &logger) {
  std::vector<pdf::PageElement> elements =
      lift_group_text(pdf::extract_page(content, *page.resources, logger));
  for (const pdf::Annotation *annotation : page.annotations) {
    std::vector<pdf::PageElement> appearance =
        lift_group_text(pdf::extract_annotation(*annotation, logger));
    elements.insert(elements.end(), std::make_move_iterator(appearance.begin()),
                    std::make_move_iterator(appearance.end()));
  }
  return elements;
}

/// Deduplicates CSS declarations into atomic, single-property classes named
/// `<prefix><n>` in first-seen order, emitted once in `<head>`. The same font
/// sizes, offsets and spacings recur across up to millions of positioned
/// elements, and inline declarations bloat the document. Representation-only:
/// no element's computed style changes.
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

  /// One rule per line (`.f1{font-size:9.96pt}`) so regeneration diffs stay
  /// legible; each is preceded by a newline.
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
  HtmlServiceImpl(PdfFile pdf_file, HtmlConfig config, const Logger &logger)
      : HtmlService(std::move(config), logger), m_pdf_file{std::move(pdf_file)},
        m_resources{locate_search_resources(this->config())} {
    // declared before any page is parsed, so before the views are known
    for (auto &&resource : locate_viewport_resources(this->config())) {
      m_resources.push_back(std::move(resource));
    }
  }

  /// Parses once, applies the `[page_range_begin, page_range_end)` range and
  /// builds the views: the combined document plus one per rendered page. The
  /// parser and its object cache live for the service's lifetime so every view
  /// renders off that one parse.
  void warmup() const override {
    std::lock_guard lock(m_mutex);

    if (m_document != nullptr) {
      return;
    }

    const auto &pdf_file =
        dynamic_cast<const pdf::PdfFile &>(*m_pdf_file.impl());
    m_parser =
        std::make_unique<pdf::DocumentParser>(pdf_file.create_parser(m_logger));
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

  [[nodiscard]] bool is_view(const std::string &path) const {
    warmup();
    return std::ranges::any_of(
        m_views, [&path](const auto &view) { return view.path() == path; });
  }

  [[nodiscard]] bool exists(const std::string &path) const override {
    return is_view(path) || resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (is_view(path)) {
      return "text/html";
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      return resource->mime_type();
    }
    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        !is_view(path) && resource != nullptr) {
      resource->write_resource(out);
      return;
    }
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

  /// One standalone page (the `page{index}.html` view). Internal links are
  /// rebased onto this page's own directory: the browser resolves an href
  /// against the current document, not the output root.
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
    const std::array<pdf::Page *, 1> pages{m_pages[page_index]};
    return write_pages(out, pages, m_first_page + page_index + 1, page_href);
  }

  HtmlResources write_pages(HtmlWriter &out,
                            const std::span<pdf::Page *const> pages,
                            const std::size_t first_page_number,
                            const PageHref &page_href) const {
    if (config().pdf_text_mode == PdfTextMode::single_layer) {
      return write_pages_single_layer(out, pages, first_page_number, page_href);
    }
    return write_pages_dual_layer(out, pages, first_page_number, page_href);
  }

  // ---- DUAL-LAYER MODE ----------------------------------------------------
  //
  // Visual layer (`.vis`, aria-hidden): paint-order glyphs in PUA-re-encoded
  // fonts, grouped into baseline line blocks (`.t`) whose runs flow inline. A
  // path or image closes the open block and goes into an SVG. Invisible text
  // (Tr 3/7) is omitted.
  //
  // Selection layer (`.sel`): transparent real Unicode in content-stream
  // order, one line block per detected line. Each run is an inline-block of
  // the PDF advance width, spread to fill it by CSS
  // `text-justify:inter-character` — no JavaScript; gaps are zero-content
  // spacer spans.

  /// One run inside a visual line block: margin-left, font size,
  /// font-family+colour. The line block holds placement only.
  struct VisRunOut {
    std::string classes;
    std::string text; ///< PUA glyph string (or real unicode for fallback path)
  };
  /// One visual line block, absolutely positioned at its first run's origin.
  struct VisLineOut {
    std::string classes; ///< "t lN tN [mN]" (or matrix transform)
    std::vector<VisRunOut> runs;
  };
  struct PathOut {
    std::string svg;
  };
  /// Visual-layer paint-order item: a glyph line block, or an SVG fragment.
  using VisItem = std::variant<VisLineOut, PathOut>;

  /// One selection-layer run: an inline-block span of fixed width (for CSS
  /// justify). Empty `text` means a spacer-only span.
  struct SelRunOut {
    std::string classes; ///< "sr wN [mlN]"
    std::string text; ///< real unicode (HTML-escaped), may be empty for spacer
  };
  /// One selection-layer line block: absolutely positioned, transparent.
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
                                       const std::span<pdf::Page *const> pages,
                                       const std::size_t first_page_number,
                                       const PageHref &page_href) const {
    HtmlResources resources;
    const WritingState state(out, config(), resources);

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
      return intern_font(family_index, family_count, font, m_logger,
                         [&](std::uint32_t) {
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

    // Strips a trailing `wN` (width) token so a merged selection run's width
    // can be re-declared. At most one is ever attached.
    const auto strip_width_class = [](std::string &classes) {
      const std::size_t pos = classes.rfind(' ');
      if (pos == std::string::npos) {
        return;
      }
      const std::string_view tail(classes.data() + pos + 1,
                                  classes.size() - pos - 1);
      if (tail.size() > 1 && tail.front() == 'w' &&
          std::ranges::all_of(tail.substr(1), [](const char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
          })) {
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

      /// index of the open VisLineOut in vis_items, -1 = none
      std::int32_t vis_cur_line = -1;
      double vis_prev_end = 0;
      double vis_prev_baseline = 0;
      double vis_prev_font_pt = 0;
      bool vis_prev_was_matrix = false;
      std::string vis_cur_flow_key;
      const auto vis_close_line = [&] { vis_cur_line = -1; };

      // Selection layer state: reading-order grouping, in the open block's
      // frame.
      bool sel_have_prev = false;
      double sel_prev_baseline = 0;
      double sel_prev_end = 0;
      /// previous run's advance height, for `starts_new_line`
      double sel_prev_font_pt = 0;
      bool sel_prev_ends_space = false;
      bool sel_prev_was_matrix = false;
      std::int32_t sel_cur_line = -1;
      /// origin the open `.sr` run starts at, to recompute its width on merge
      double sel_cur_run_start_ox = 0;
      /// font-size of the previous element, for its line's trailing space
      double sel_prev_font_size_pt = 0;
      /// the open block's transform: its linear part identifies runs one CSS
      /// matrix can place, its origin anchors the frame they flow in
      util::math::Transform2D sel_block;
      /// advance owed to the next run: whitespace no span could carry, plus a
      /// gap no spacer took
      double sel_pending_space = 0;

      for (const pdf::PageElement &element :
           page_elements(*page, stream, m_logger)) {
        if (handle_graphic_element(
                element, to_box, width, height, clips, gradients, patterns,
                masks, m_logger, [&] { vis_close_line(); },
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
        // Invisible runs paint nothing and Type3 runs are painted by their char
        // procs, so both contribute to the selection layer only.
        if (!invisible && !text.render_as_graphics) {
          // A block carries its first run's placement and the rest flow off it,
          // so another font, size or ascent needs a block of its own.
          std::ostringstream key;
          key << font << '|' << font_size_pt << '|' << round2(asc * text.size);
          if (font == 0 && text.font != nullptr && text.font->substitute) {
            key << '|' << font_substitute_declaration(*text.font->substitute);
          }
          const std::string vis_flow_key = std::move(key).str();

          bool new_vis_line = is_matrix || vis_prev_was_matrix ||
                              vis_cur_line < 0 ||
                              vis_flow_key != vis_cur_flow_key;
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
            std::string line_base = "t";
            add_position_classes(line_base, add_class, m, is_matrix, ox,
                                 baseline, asc * text.size);
            VisLineOut line_out;
            line_out.classes = std::move(line_base);
            page_out.vis_items.push_back(std::move(line_out));
            vis_cur_line = static_cast<int>(page_out.vis_items.size()) - 1;
            vis_cur_flow_key = vis_flow_key;
          }

          std::string run_classes = "g"; // user-select:none
          add_class(run_classes, "f", pt_decl("font-size", font_size_pt));
          if (font != 0) {
            run_classes += ' ';
            run_classes += font_class(font_class_used, font, invisible);
          } else if (text.font != nullptr && text.font->substitute) {
            // Non-embedded: real Unicode in the substitute family, whose
            // metric-overriding face pins the baseline to `asc`.
            add_class(
                run_classes, "ff",
                substitute_faces.declaration(*text.font->substitute, asc));
          }
          if (vis_margin_pt != 0) {
            add_class(run_classes, "ml", pt_decl("margin-left", vis_margin_pt));
          }
          run_classes += color_suffix;

          std::string run_text;
          if (font != 0) {
            // Not `escape_text`: its `&nbsp;` is a different character, which
            // `word-spacing` does not move.
            run_text = escape_markup(glyph_run_str(*text.font, text.codes));
          } else {
            // `margin-left` already spans the word break; rendering it too
            // shifts the glyphs by a space, once per run.
            run_text = escape_text(core_text(text));
          }

          if (const double cs_pt = round2(text.char_spacing * scale);
              cs_pt != 0) {
            add_class(run_classes, "s", pt_decl("letter-spacing", cs_pt));
          }
          // Tw applies to single-byte code 32 alone (9.3.3), which only a
          // simple font's run keeps as a real U+0020 (`space_glyph`).
          if (!(text.font != nullptr && text.font->composite)) {
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
        // A run is measured in the frame its block lays out in: page space for
        // an axis-aligned block, the block's own space for a matrix one. Both
        // put x along the writing line, so one set of tests serves both.
        if (!text.text.empty()) {
          const bool starts_space = text.text.front() == ' ';
          // A leading inferred space is dropped: the gap between runs is
          // covered by the spacer span, not by the run text.
          std::string core = starts_space ? text.text.substr(1) : text.text;

          const double tz = text.horizontal_scaling / 100.0;
          // In the block's frame the unit is text space: `text.width` carries
          // the horizontal scaling the CSS matrix applies again.
          const double local_extent = tz != 0 ? text.width / tz : 0;
          double sel_ox = is_matrix ? 0 : ox;
          double sel_baseline = is_matrix ? 0 : baseline;
          const double sel_extent = is_matrix ? local_extent : extent;
          const double sel_font_pt = is_matrix ? text.size : font_pt;

          bool sel_frame_kept =
              sel_have_prev && !is_matrix && !sel_prev_was_matrix;
          if (is_matrix && sel_cur_line >= 0 && sel_prev_was_matrix &&
              same_linear(sel_block, m)) {
            if (const std::optional<std::array<double, 2>> local =
                    local_origin(sel_block, m)) {
              sel_ox = (*local)[0];
              sel_baseline = (*local)[1];
              sel_frame_kept = true;
            }
          }

          const double width_pt = round2(sel_extent);
          const double gap_pt = std::max(0.0, sel_ox - sel_prev_end);

          bool new_sel_line = !sel_frame_kept;
          bool sel_gap = false;
          if (sel_frame_kept && sel_prev_font_pt > 0) {
            new_sel_line =
                starts_new_line(sel_baseline, sel_prev_baseline, sel_ox,
                                sel_prev_end, sel_prev_font_pt);
            sel_gap = sel_ox - sel_prev_end > 0.25 * sel_prev_font_pt;
          }
          // A block anchors its runs to one baseline, so a run a rise lifts
          // off it — a superscript — needs its own, though it breaks nothing.
          const bool baseline_shift =
              is_matrix && sel_frame_kept && !new_sel_line &&
              sel_prev_font_pt > 0 &&
              std::abs(sel_baseline - sel_prev_baseline) >
                  0.02 * sel_prev_font_pt;
          new_sel_line = new_sel_line || baseline_shift;
          // The extractor's leading space is the break; a block opened because
          // the frames are not comparable is not.
          const bool break_space =
              (sel_frame_kept && !baseline_shift) || starts_space;

          if (new_sel_line) {
            // The block starts here, so this run sits at its origin.
            sel_ox = is_matrix ? 0 : ox;
            sel_baseline = is_matrix ? 0 : baseline;
            sel_block = m;
            // Close the previous line with a trailing space. `sg`, not `sr`:
            // it carries no PDF-derived width, just the space.
            if (sel_cur_line >= 0 && sel_have_prev && !sel_prev_ends_space &&
                break_space) {
              std::string space_cls = "sg";
              add_class(space_cls, "f",
                        pt_decl("font-size", sel_prev_font_size_pt));
              page_out.sel_lines[sel_cur_line].runs.push_back(
                  SelRunOut{std::move(space_cls), " "});
            }
            std::string sel_base = "t";
            add_position_classes(sel_base, add_class, m, is_matrix, ox,
                                 baseline, asc * text.size);
            sel_base += " i"; // transparent
            page_out.sel_lines.push_back(SelLineOut{std::move(sel_base), {}});
            sel_cur_line = static_cast<int>(page_out.sel_lines.size()) - 1;
            // Nothing is owed at the origin — unless this run is whitespace,
            // which emits no span to carry its advance.
            sel_pending_space = core.empty() ? sel_extent : 0;
            if (!core.empty()) {
              std::string cls = "sr";
              add_class(cls, "f", pt_decl("font-size", font_size_pt));
              if (width_pt > 0) {
                add_class(cls, "w", pt_decl("width", width_pt));
              }
              page_out.sel_lines[sel_cur_line].runs.push_back(
                  SelRunOut{std::move(cls), escape_markup(std::move(core))});
              sel_cur_run_start_ox = sel_ox;
            }
          } else if (sel_gap || sel_prev_ends_space || starts_space) {
            std::vector<SelRunOut> &runs =
                page_out.sel_lines[sel_cur_line].runs;
            // The gap before this run, plus its own advance when it is only
            // whitespace and emits no `.sr`. Dropping either shortens the line.
            sel_pending_space += gap_pt + (core.empty() ? sel_extent : 0);
            // One space character per run of whitespace; a second would be
            // copied as one. The advance waits for the next `.sr` instead.
            if (!sel_prev_ends_space && !runs.empty()) {
              std::string gap_cls = "sg";
              add_class(gap_cls, "f", pt_decl("font-size", font_size_pt));
              const double rounded_gap = round2(sel_pending_space);
              if (rounded_gap > 0) {
                add_class(gap_cls, "w", pt_decl("width", rounded_gap));
                // Only a gap that still reads as a word space: a column of
                // white painted solid is worse than the sliver.
                if (rounded_gap <= font_size_pt) {
                  gap_cls += " sw";
                }
              }
              runs.push_back(SelRunOut{std::move(gap_cls), " "});
              sel_pending_space = 0;
            }
            if (!core.empty()) {
              std::string cls = "sr";
              add_class(cls, "f", pt_decl("font-size", font_size_pt));
              if (const double owed = round2(sel_pending_space); owed > 0) {
                add_class(cls, "ml", pt_decl("margin-left", owed));
              }
              sel_pending_space = 0;
              if (width_pt > 0) {
                add_class(cls, "w", pt_decl("width", width_pt));
              }
              runs.push_back(
                  SelRunOut{std::move(cls), escape_markup(std::move(core))});
              sel_cur_run_start_ox = sel_ox;
            }
          } else {
            // Tight continuation on the same baseline: merge into the previous
            // run so the browser reads the sequence as one word, and widen that
            // run to the full merged extent so CSS justify still spreads across
            // the true PDF advance.
            std::vector<SelRunOut> &runs =
                page_out.sel_lines[sel_cur_line].runs;
            if (!runs.empty()) {
              runs.back().text += escape_markup(text.text);
              strip_width_class(runs.back().classes);
              const double merged_width_pt =
                  round2(sel_ox + sel_extent - sel_cur_run_start_ox);
              if (merged_width_pt > 0) {
                add_class(runs.back().classes, "w",
                          pt_decl("width", merged_width_pt));
              }
            }
          }

          sel_prev_baseline = sel_baseline;
          sel_prev_end = sel_ox + sel_extent;
          sel_prev_font_pt = sel_font_pt;
          sel_prev_ends_space = !text.text.empty() && text.text.back() == ' ';
          sel_prev_was_matrix = is_matrix;
          sel_prev_font_size_pt = font_size_pt;
          sel_have_prev = true;
        }
      }

      // Selection lines stay in content-stream order: sorting by baseline y
      // would interleave multi-column layouts, which the stream keeps
      // contiguous. The fix is page segmentation (XY-cut), not a scalar sort.
      page_out.clip_defs =
          clips.defs() + gradients.defs() + patterns.defs() + masks.defs();
    }

    // Post-pass: re-encode accepted fonts PUA-only.
    for (std::uint32_t i = 0; i < family_count; ++i) {
      write_font_face(*accepted_fonts[i], i, {}, font_class_used[i], font_faces,
                      font_styles);
    }
    substitute_faces.append_faces(font_faces);

    const std::optional<double> content = content_pixels(pages_out, config());
    write_header_common(state, font_faces, font_styles, styles, content, [&] {
      // Visual layer glyph spans: not selectable (selection rides the `.sel`
      // layer).
      out.out() << ".g{user-select:none}";
      // Selection-layer fallback font: `size-adjust` shrinks a local system
      // font under the PDF-derived `.sr`/`.sg` widths. CSS justify only ever
      // *adds* spacing, so undershooting is free while overshooting overflows
      // and is clipped — hence the deliberately low config default. With no
      // fonts configured `.i` falls through to plain `sans-serif`.
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
      // Selection-layer run span. `overflow:hidden` clips a wider system font;
      // `.t`'s inherited `pre` blocks wrapping while preserving a run's own
      // leading/trailing space, which is real PDF content.
      out.out() << ".sr{display:inline-block;text-align:justify;"
                   "text-align-last:justify;text-justify:inter-character;"
                   "overflow:hidden}";
      // Selection-layer gap spacer. `overflow:hidden` matches `.sr`: an
      // inline-block baseline-aligns to its bottom margin edge only when
      // overflow isn't visible, so without it the spacer shifts in y.
      out.out() << ".sg{display:inline-block;overflow:hidden}";
      // A lone space cannot be justified to its box, so pad the advance and let
      // the width clip it: else every word break shows a sliver of white.
      out.out() << ".sw{letter-spacing:1000pt}";
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
    out.write_element_begin("div", HtmlElementOptions().set_class("d"));
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
    out.write_element_end("div"); // .d
    write_search_script(state);
    write_viewport_script(state);
    out.write_body_end();
    out.write_end();

    return resources;
  }

  // ---- SINGLE-LAYER MODE --------------------------------------------------
  //
  // One combined text layer per page: runs grouped into paint-order line
  // blocks (`.t`), each run nudged by a `margin-left` gap.
  //
  // The embedded font's cmap is built by frequency analysis — a pre-pass counts
  // (uchar, glyph) co-occurrences per font and the winner takes the entry — so
  // the common shape wins instead of whichever run came first.
  //
  // A run whose pairs all match the winner ("clean") renders real Unicode
  // directly in the embedded font, natively findable. An unclean run paints
  // glyphs via `::before{content:attr(data-g)}` — out of the DOM text stream,
  // so find never breaks mid-word — with a zero-width `.ov` overlay carrying
  // the Unicode. Invisible and fallback runs render Unicode as ordinary text.

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

  HtmlResources write_pages_single_layer(
      HtmlWriter &out, const std::span<pdf::Page *const> pages,
      const std::size_t first_page_number, const PageHref &page_href) const {
    HtmlResources resources;
    const WritingState state(out, config(), resources);

    pdf::DocumentParser &parser = *m_parser;
    LinkResolver &link_resolver = *m_link_resolver;

    // A real-Unicode scalar gets a cmap entry only inside the BMP and outside
    // the PUA, so the glyph-deterministic PUA code points are never shadowed.
    const auto collapsible_unicode = [](const char32_t c) {
      return c <= 0xFFFF && !(c >= 0xE000 && c <= 0xF8FF);
    };

    // A leading inferred space carries no character code or advance, so the 1:1
    // codes-to-text alignment starts after it — as `core_text_begin` views it.
    const auto core_char_count = [](const pdf::TextElement &t) {
      return util::string::utf8_length(t.text) -
             (t.leading_space_inferred ? 1u : 0u);
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
      return intern_font(family_index, family_count, font, m_logger,
                         [&](std::uint32_t) {
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
    // Every page is extracted twice (here and in the main pass): re-parsing an
    // already-decoded stream is cheaper than buffering every page's elements.
    for (std::size_t pi = 0; pi < pages.size(); ++pi) {
      const pdf::Page &page = *pages[pi];
      for (const pdf::PageElement &element :
           page_elements(page, page_streams[pi], m_logger)) {
        const auto *text = std::get_if<pdf::TextElement>(&element);
        if (text == nullptr || text->text.empty() || text->font == nullptr) {
          continue;
        }
        const std::uint32_t font = font_family(text->font);
        if (font == 0) {
          continue;
        }
        // Only collapsible-candidate runs vote; the leading inferred space is
        // skipped so a run carrying one still does.
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

    // The winning glyph per (font, uchar) takes the cmap entry; the map's
    // ascending glyph order makes strict `>` break ties by lower glyph id.
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

      for (const pdf::PageElement &element :
           page_elements(page, page_streams[pi], m_logger)) {
        if (handle_graphic_element(
                element, to_box, width, height, clips, gradients, patterns,
                masks, m_logger, [&] { close_line(); },
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

        SingleRunOut run;
        // `color_suffix` leads with a space for the dual-layer paths that
        // concatenate it; a single-layer run stores the bare class name.
        run.color =
            color_suffix.empty() ? std::string() : color_suffix.substr(1);

        if (font == 0 || invisible) {
          // Fallback / invisible: real unicode directly. The word break rides
          // its own span as below, else it is margined *and* advanced over.
          run.lead_space = text.leading_space_inferred;
          run.text = escape_markup(core_text(text));
        } else {
          // Collapse needs 1:1 codes-to-core-text and every (uchar, glyph) to
          // match the frequency winner. The leading inferred space is metadata,
          // not a coded char, so excluding it keeps a recovered word break from
          // forcing the whole run onto the PUA path.
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
            // A leading inferred space becomes its own span, never a literal
            // space in `text`, so `white-space:pre` cannot shift the glyphs off
            // their placement origin. Over a positive gap that span carries the
            // gap as its width (pdf2htmlEX's model: one real space that is both
            // the copyable character and the advance), else it is zero-width.
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
        // The substitute family is part of the flow key, so a Helvetica run and
        // a Times run never share one line block's `font_class`.
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
            // the space span rather than a `margin-left` on the run.
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
    const std::optional<double> content = content_pixels(pages_out, config());
    write_header_common(state, font_faces, font_styles, styles, content, [&] {
      // Invisible text render modes (Tr 3/7).
      out.out() << ".i{color:transparent}";
      // Unclean glyphs via generated content, out of the DOM text stream.
      out.out() << ".gl::before{content:attr(data-g)}";
      // The real Unicode of an unclean run: invisible and zero-width, but still
      // found and selected in reading order. `inline-block` is what lets
      // `width:0` apply at all.
      out.out() << ".ov{display:inline-block;width:0;overflow:hidden;"
                   "color:transparent;vertical-align:baseline}";
      // Width-bearing selectable space for a recovered word break: a real `" "`
      // sized to the gap by a `wN` class, copyable *and* carrying the advance.
      // Deliberately no `overflow:hidden`: it would move the inline-block's
      // baseline to its bottom margin edge and highlight the space at the wrong
      // height, while clipping nothing (the space is transparent).
      out.out() << ".sp{display:inline-block;"
                   "color:transparent;vertical-align:baseline}";
      // A hit in the overlay is clipped away with it, so the glyphs it belongs
      // to carry the highlight instead - the whole run of them, which is as
      // narrow as the overlay can say.
      out.out() << ".gl:has(+.ov mark){background:#ff0;"
                   "mix-blend-mode:multiply}";
      out.out() << ".gl:has(+.ov mark.current){background:orange}";
    });

    // A run's span class: `head` plus its optional margin-left and colour.
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
            // With a `wN` width the space carries the gap; without one it is a
            // zero-width `.ov` (line-leading or negative gap) that cannot shift
            // the glyphs under `white-space:pre`.
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
    out.write_element_begin("div", HtmlElementOptions().set_class("d"));
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
      out.write_element_end("div"); // .p
    }
    out.write_element_end("div"); // .d
    write_search_script(state);
    write_viewport_script(state);
    out.write_body_end();
    out.write_end();

    return resources;
  }

  static std::string pt_decl(const char *property, double value) {
    std::ostringstream s;
    s << property << ':' << value << "pt";
    return std::move(s).str();
  }

  /// Whether a run at (`ox`, `baseline`) starts a new visual line: its baseline
  /// jumped by more than 0.6× the previous run's advance height, or its origin
  /// sits left of that run's right edge — a carriage return. Shared by all
  /// three layers so the heuristic cannot drift between them; callers gate on
  /// `prev_font_pt > 0`.
  static bool starts_new_line(const double baseline, const double prev_baseline,
                              const double ox, const double prev_end,
                              const double prev_font_pt) {
    return std::abs(baseline - prev_baseline) > 0.6 * prev_font_pt ||
           ox < prev_end - 0.5 * prev_font_pt;
  }

  /// Whether two runs share a linear part, so one line block's CSS matrix
  /// places both.
  static bool same_linear(const util::math::Transform2D &l,
                          const util::math::Transform2D &r) {
    return l.a == r.a && l.b == r.b && l.c == r.c && l.d == r.d;
  }

  /// `m`'s origin along `block`'s own axes, x along the writing line. Empty
  /// for a singular linear part, which spans no frame.
  static std::optional<std::array<double, 2>>
  local_origin(const util::math::Transform2D &block,
               const util::math::Transform2D &m) {
    const double det = block.a * block.d - block.b * block.c;
    if (det == 0) {
      return std::nullopt;
    }
    const double dx = m.e - block.e;
    const double dy = m.f - block.f;
    return std::array<double, 2>{(dx * block.d - dy * block.c) / det,
                                 (-dx * block.b + dy * block.a) / det};
  }

  /// Per-run geometry from a `TextElement` and the page's `to_box`. Identical
  /// in every text mode, and kept in one place so no call site can drift.
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
    // `m.a > 0` keeps a pure 180° rotation (a = d = -1) off the axis-aligned
    // fast path, where it would feed a negative `m.a` into the placement math.
    const bool is_matrix = !(m.b == 0 && m.c == 0 && m.a == m.d && m.a > 0);
    const double tz = text.horizontal_scaling / 100.0;
    const double axis = tz != 0 ? std::hypot(m.a, m.b) / tz : 0;
    return RunGeometry{
        .m = m,
        .invisible = invisible,
        .is_matrix = is_matrix,
        .asc = ascent_em(text.font),
        .scale = is_matrix ? 1.0 : m.a,
        .ox = m.e,
        .baseline = m.f,
        .extent = text.width * axis,
        .font_pt = text.size * axis,
        .font_size_pt = round2(is_matrix ? text.size : m.a * text.size),
    };
  }

  /// The colour class suffix (with a leading space) for a run's paint colour,
  /// or "" for black / invisible.
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
    // The span carries one opacity, so a fill-and-stroke glyph takes the more
    // opaque of the two: a transparent fill must not drop an opaque outline.
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

  /// The page-box geometry shared by both modes' page setup; `add_class`
  /// interns the width/height declarations.
  struct PageBox {
    double width;
    double height;
    util::math::Transform2D to_box;
    std::string classes;
  };

  template <typename AddClass>
  static PageBox begin_page(const pdf::Page &page, AddClass &&add_class) {
    // The crop box is what a viewer shows (14.11.2); it falls back to the media
    // box. Its corners may be given in either order (7.9.5).
    const pdf::Array &page_box = page.crop_box.as_array();
    const double box_x0 =
        std::min(page_box[0].as_real(), page_box[2].as_real());
    const double box_y0 =
        std::min(page_box[1].as_real(), page_box[3].as_real());
    const double box_width =
        std::max(page_box[0].as_real(), page_box[2].as_real()) - box_x0;
    const double box_height =
        std::max(page_box[1].as_real(), page_box[3].as_real()) - box_y0;

    // `/Rotate` turns the page clockwise as displayed (7.7.3.3), so a quarter
    // turn swaps what the reader sees as its width and height.
    const bool quarter_turn = page.rotate == 90 || page.rotate == 270;
    const double width = quarter_turn ? box_height : box_width;
    const double height = quarter_turn ? box_width : box_height;

    std::string classes = "p";
    {
      std::ostringstream w;
      w << "width:" << width * pt_to_in << "in";
      add_class(classes, "x", std::move(w).str());
      std::ostringstream h;
      h << "height:" << height * pt_to_in << "in";
      add_class(classes, "y", std::move(h).str());
    }

    // Onto the displayed box: 90° sends the top-left corner to the top-right,
    // 270° to the bottom-left.
    const util::math::Transform2D rotation = [&]() -> util::math::Transform2D {
      switch (page.rotate) {
      case 90:
        return {0, 1, -1, 0, box_height, 0};
      case 180:
        return {-1, 0, 0, -1, box_width, box_height};
      case 270:
        return {0, -1, 1, 0, 0, box_width};
      default:
        return {};
      }
    }();

    const util::math::Transform2D to_box =
        util::math::Transform2D::translation(-box_x0, -box_y0) *
        util::math::Transform2D::scaling_translation(1, -1, 0, box_height) *
        rotation;

    return {width, height, to_box, std::move(classes)};
  }

  /// The 1-based font family index for `font`, or 0 when it is unusable. Runs
  /// `on_accept(index)` on first acceptance so the caller can grow its parallel
  /// per-font arrays.
  template <typename OnAccept>
  static std::uint32_t intern_font(
      std::unordered_map<const pdf::Font *, std::uint32_t> &family_index,
      std::uint32_t &family_count, const pdf::Font *font, const Logger &logger,
      OnAccept &&on_accept) {
    const auto [it, inserted] = family_index.try_emplace(font, 0);
    if (!inserted) {
      return it->second;
    }
    if (!font_is_usable(*font, logger)) {
      return 0;
    }
    const std::uint32_t index = ++family_count;
    it->second = index;
    on_accept(index);
    return index;
  }

  /// A page's `<defs>` and its paint-order body: an SVG open/close dance around
  /// the item list, identical in both modes bar the line and path types.
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

  /// The widest page a view holds, in css pixels, with `.d`'s side gutters.
  template <typename PageOut>
  static std::optional<double> content_pixels(const std::vector<PageOut> &pages,
                                              const HtmlConfig &config) {
    double widest = 0;
    for (const PageOut &page : pages) {
      widest = std::max(widest, page.width);
    }
    if (widest <= 0) {
      return {};
    }
    return widest * pt_to_in * 96.0 + page_column_gutter_pixels(config);
  }

  /// The document/head prologue shared by both modes, with `write_mode_css()`
  /// slotted between the constant rules. Leaves the writer after `</head>`.
  template <typename WriteModeCss>
  void write_header_common(const WritingState &state,
                           const std::string &font_faces,
                           const std::string &font_styles,
                           const AtomicStyles &styles,
                           const std::optional<double> content,
                           WriteModeCss &&write_mode_css) const {
    HtmlWriter &out = state.out();

    out.write_begin();
    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    write_viewport_meta(out, config(), true);
    write_zoom_style(out, config(), width_fit(config(), true), content);
    write_content_margin_style(out, config());
    out.write_header_style_begin();
    out.out() << "body{margin:0;background:#525659}";
    // `.d`: the page column, sized to the widest page so pages of differing
    // width centre against each other rather than against the viewport. Their
    // side margin is part of that width, so a phone screen keeps a gutter.
    out.out() << ".d{display:flex;flex-direction:column;align-items:center;"
                 "gap:16px;padding:max(16px,var(--odr-min-margin-top,0px)) 0 "
                 "max(16px,var(--odr-min-margin-bottom,0px));"
                 "width:max-content;min-width:100%}";
    // `overflow:hidden` clips to the crop box, as a viewer does: content may
    // sit outside it (a bleed, or an InDesign spread's other page).
    out.out() << ".p{position:relative;"
                 "margin:0 max(16px,var(--odr-min-margin-right,0px)) 0 "
                 "max(16px,var(--odr-min-margin-left,0px));background:#fff;"
                 "overflow:hidden;box-shadow:0 1px 4px rgba(0,0,0,.5)}";
    // `.t`: shared base for all absolutely-positioned line blocks.
    // `font-size:0` collapses its strut, which outranks the run it holds and
    // would take the line box's baseline.
    out.out() << ".t{position:absolute;left:0;top:0;transform-origin:0 0;"
                 "white-space:pre;line-height:1;font-size:0;font-kerning:none;"
                 "font-variant-ligatures:none}";
    write_mode_css();
    // SVG overlay covering the page box (visual graphics layer).
    out.out() << ".s{position:absolute;left:0;top:0;width:100%;height:100%;"
                 "overflow:hidden;pointer-events:none}";
    // Link annotation overlays (absolutely positioned in page-box points).
    out.out() << ".lk{position:absolute;transform-origin:0 0}";
    // A search hit marks the text layer over the page: the browser's own `mark`
    // colour would paint invisible text, an opaque highlight hide the glyphs.
    out.out() << "mark{color:inherit;mix-blend-mode:multiply}";
    out.out() << font_faces;
    out.out() << font_styles;
    styles.write_rules(out.out());
    out.write_header_style_end();
    write_search_style(state);
    out.write_header_end();
  }

  /// Appends a line block's placement classes: `l`/`t` (left/top in pt) for an
  /// axis-aligned run, `m` (a transform re-anchored to the baseline by
  /// `ascent_pt`) for a rotated or skewed one. Shared by all three layers.
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
    // A CSS `matrix()` translation is intrinsically px; `translate()` takes pt
    // and applies outside it, so the whole text layer stays in pt.
    const double tx = m.e - m.c * ascent_pt;
    const double ty = m.f - m.d * ascent_pt;
    std::ostringstream t;
    t << "transform:translate(" << round2(tx) << "pt," << round2(ty)
      << "pt) matrix(" << m.a << ',' << m.b << ',' << m.c << ',' << m.d
      << ",0,0)";
    add_class(classes, "m", std::move(t).str());
  }

  /// Serializes `sfnt` re-encoded to the PUA, restoring the cmap
  /// `reencode_to_pua` overwrites — the `SfntFont` is shared by every page, and
  /// a left-behind PUA cmap makes the next `glyph_for_code` miss.
  static std::string
  write_sfnt_pua(font::sfnt::SfntFont &sfnt,
                 const std::map<char32_t, std::uint16_t> &extra_unicode) {
    std::map<char32_t, std::uint16_t> original_cmap = sfnt.cmap();
    try {
      font::reencode_to_pua(sfnt, extra_unicode);
      std::string reencoded = sfnt.write();
      sfnt.set_cmap(std::move(original_cmap));
      return reencoded;
    } catch (...) {
      sfnt.set_cmap(std::move(original_cmap));
      throw;
    }
  }

  /// Whether `font`'s embedded program re-encodes without throwing. Probes the
  /// real encode path so failures surface here, not in the post-pass. Failing
  /// swaps in a substitute, which the page shows, so say so.
  static bool font_is_usable(const pdf::Font &font, const Logger &logger) {
    const auto dropped = [&](const std::string &why) {
      ODR_WARNING(logger, "pdf: rendering '"
                              << font.embedded_font->name()
                              << "' with a substitute, its embedded program "
                                 "does not re-encode: "
                              << why);
      return false;
    };
    if (const auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
            font.embedded_font)) {
      try {
        (void)write_sfnt_pua(*sfnt, {});
        return true;
      } catch (const std::exception &e) {
        return dropped(e.what());
      } catch (...) {
        return dropped("sfnt re-encode failed");
      }
    }
    if (const auto cff =
            std::dynamic_pointer_cast<font::cff::CffFont>(font.embedded_font)) {
      try {
        (void)font::cff::wrap_to_otf(*cff);
        return true;
      } catch (const std::exception &e) {
        return dropped(e.what());
      } catch (...) {
        return dropped("cff wrap failed");
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

  /// Re-encodes `font`'s embedded program, folding `extra_unicode`'s cmap
  /// entries in alongside the PUA range, and appends its `@font-face` plus the
  /// `.fvN`/`.fnN` rules `class_used` says are needed.
  static void write_font_face(const pdf::Font &font, const std::uint32_t index,
                              std::map<char32_t, std::uint16_t> extra_unicode,
                              const std::array<bool, 2> &class_used,
                              std::string &font_faces,
                              std::string &font_styles) {
    if (const std::uint16_t space = space_glyph(font); space != 0) {
      extra_unicode.emplace(U' ', space);
    }
    std::string reencoded;
    if (const auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
            font.embedded_font)) {
      reencoded = write_sfnt_pua(*sfnt, extra_unicode);
    } else if (const auto cff = std::dynamic_pointer_cast<font::cff::CffFont>(
                   font.embedded_font)) {
      reencoded = font::cff::wrap_to_otf(*cff, extra_unicode);
    }
    const std::string url = file_to_url(reencoded, "font/ttf");
    const std::string n = std::to_string(index + 1);
    // The overrides sum to one em, so `line-height:1` puts the baseline at
    // exactly the `ascent_em` a run's `top` is derived from.
    const double ascent = ascent_em(&font);
    std::ostringstream face;
    face << "@font-face{font-family:'odr-f" << n << "';src:url(" << url
         << ");ascent-override:" << round2(ascent * 100.0)
         << "%;descent-override:" << round2((1.0 - ascent) * 100.0)
         << "%;line-gap-override:0%}";
    font_faces += std::move(face).str();
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

  /// Baseline offset below a line block's `top`, in em. Capped at one em so
  /// the `@font-face` descent can make the two sum to it.
  static double ascent_em(const pdf::Font *font) {
    double em = 0.8;
    if (font != nullptr && font->descriptor_ascent) {
      em = *font->descriptor_ascent;
    } else if (font != nullptr && font->embedded_font != nullptr) {
      const std::uint16_t units = font->embedded_font->units_per_em();
      if (units != 0) {
        em = static_cast<double>(font->embedded_font->bounding_box().y_max) /
             units;
      }
    }
    return std::clamp(em, 0.5, 1.0);
  }

  /// The glyph a simple font paints for byte 32, 0 when it has none. That byte
  /// keeps a real U+0020 for CSS `word-spacing` to move, and `write_font_face`
  /// maps U+0020 onto this glyph so the painted shape is unchanged.
  static std::uint16_t space_glyph(const pdf::Font &font) {
    if (font.composite || font.embedded_font == nullptr) {
      return 0;
    }
    const std::uint16_t glyph = font.glyph_for_code(' ');
    return glyph < font.embedded_font->glyph_count() ? glyph : 0;
  }

  static std::string glyph_run_str(const pdf::Font &font,
                                   const std::string &codes) {
    const std::uint16_t space = space_glyph(font);
    std::string s;
    for (const std::uint32_t code : font.codes(codes)) {
      if (code == ' ' && space != 0) {
        s += ' ';
        continue;
      }
      util::string::append_c32(font::pua_code_point(font.glyph_for_code(code)),
                               s);
    }
    return s;
  }

  /// `text.text` past an inferred leading space, which backs no advance.
  static std::string::const_iterator
  core_text_begin(const pdf::TextElement &text) {
    auto cp = text.text.begin();
    if (text.leading_space_inferred) {
      utf8::unchecked::next(cp); // skip the one-byte U+0020
    }
    return cp;
  }

  static std::string core_text(const pdf::TextElement &text) {
    return std::string(core_text_begin(text), text.text.end());
  }

  /// Escapes only the three markup-significant characters. Deliberately *not*
  /// `html::escape_text`: its `&nbsp;` substitution is a distinct character
  /// from U+0020 and breaks the find and word selection these layers exist for.
  static std::string escape_markup(std::string s) {
    util::string::replace_all(s, "&", "&amp;");
    util::string::replace_all(s, "<", "&lt;");
    util::string::replace_all(s, ">", "&gt;");
    return s;
  }

  /// Handles the non-text elements common to both modes, calling `close_line`
  /// and `push_svg` for a non-empty fragment. Returns false for a text element.
  template <typename CloseLine, typename PushSvg>
  static bool
  handle_graphic_element(const pdf::PageElement &element,
                         const util::math::Transform2D &to_box, double width,
                         double height, ClipRegistry &clips,
                         GradientRegistry &gradients, PatternRegistry &patterns,
                         MaskRegistry &masks, const Logger &logger,
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
  /// The search css and js every view links; empty of locations when the config
  /// embeds them.
  HtmlResources m_resources;

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

HtmlService html::create_pdf_service(const PdfFile &pdf_file, HtmlConfig config,
                                     const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(pdf_file, std::move(config), logger));
}

} // namespace odr::internal
