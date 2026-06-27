#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <odr/exceptions.hpp>
#include <odr/logger.hpp>

#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/font/type1_font.hpp>
#include <odr/internal/font/type1_transform.hpp>
#include <odr/internal/pdf/pdf_cmap_parser.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>
#include <odr/internal/pdf/pdf_filter.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <cctype>
#include <istream>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace odr::internal::pdf {

namespace {

struct State {
  State(DocumentParser &parser, Document &document)
      : m_parser(&parser), m_document(&document) {}

  DocumentParser &parser() const { return *m_parser; }
  Document &document() const { return *m_document; }

  [[nodiscard]] XObject *find_x_object(const ObjectReference &reference) const {
    const auto it = m_x_objects.find(reference);
    return it != m_x_objects.end() ? it->second : nullptr;
  }
  void cache_x_object(const ObjectReference &reference, XObject *x_object) {
    m_x_objects[reference] = x_object;
  }

  [[nodiscard]] Font *find_font(const ObjectReference &reference) const {
    const auto it = m_fonts.find(reference);
    return it != m_fonts.end() ? it->second : nullptr;
  }
  void cache_font(const ObjectReference &reference, Font *font) {
    m_fonts[reference] = font;
  }

private:
  DocumentParser *m_parser{};
  Document *m_document{};

  /// Memoized XObject elements by pointer. A shared form/image XObject (e.g.
  /// a header reused on every page) is parsed once, and a cyclic form reference
  /// resolves to the existing — possibly still-being-built — element instead of
  /// recursing forever, so the in-memory graph mirrors the file (cycles
  /// included). `find_x_object` returns `nullptr` when not yet parsed;
  /// `cache_x_object` must register the element *before* its `/Resources` are
  /// parsed. The drawing side guards against the resulting cycles.
  std::map<ObjectReference, XObject *> m_x_objects;

  /// Memoized Font elements by reference. Fonts are shared by indirect
  /// reference across every page that uses them, so without this each page's
  /// `/Resources` would parse a fresh `Font` element. The HTML writer dedups
  /// embedded `@font-face` rules by `Font` pointer, so duplicate elements would
  /// re-inline the (base64) font program once per page — a multi-page document
  /// reusing one font would balloon to gigabytes.
  std::map<ObjectReference, Font *> m_fonts;
};

/// Normalize /Rotate to {0, 90, 180, 270}: the spec requires a multiple of 90,
/// but real files carry 360 and negatives.
Integer normalize_rotate(Integer rotate) {
  rotate = ((rotate % 360) + 360) % 360;
  return (rotate / 90) * 90;
}

/// The page attributes that are inherited through the `Pages` tree
/// (ISO 32000-1 7.7.3.3, Table 30: exactly `Resources`, `MediaBox`,
/// `CropBox`, `Rotate`). Threaded down the tree instead of walking `Parent`
/// pointers — no cycle risk, no re-reads. Each slot holds the value set by the
/// nearest ancestor that carried it (possibly an indirect reference, resolved
/// lazily at the leaf); a null slot means no ancestor set it.
struct PageAttributes {
  /// Default page size used when no `MediaBox` is present anywhere in the
  /// page tree (US Letter, 612 × 792 pt). The spec requires `MediaBox`, so
  /// this is a lenience for malformed files. (`Object` is not a literal type,
  /// hence `static const` rather than `constexpr`.)
  static const Object &default_media_box() {
    static const auto value =
        Object(Array({Object(Integer(0)), Object(Integer(0)),
                      Object(Integer(612)), Object(Integer(792))}));
    return value;
  }

  Object resources;
  Object media_box;
  Object crop_box;
  Object rotate;

  /// Overlay this node's own inheritable entries from `dictionary`. A
  /// present-but-null entry counts as absent (7.3.9: null is equivalent to
  /// omitting the entry) and leaves the inherited value untouched.
  void overlay(const Dictionary &dictionary) {
    const auto take = [&](Object &slot, const std::string &key) {
      if (dictionary.has_value(key)) {
        slot = dictionary[key];
      }
    };
    take(resources, "Resources");
    take(media_box, "MediaBox");
    take(crop_box, "CropBox");
    take(rotate, "Rotate");
  }

  /// Resolve the accumulated attributes into final values (7.7.3.4): write the
  /// resolved `media_box`/`crop_box`/`rotate` onto `page` (references
  /// resolved, missing `MediaBox` → US Letter, `CropBox` → `MediaBox`,
  /// `Rotate` normalized) and return the `Resources` object (resolved, or an
  /// empty dictionary if absent) for the caller to parse.
  Object resolve_into(Page &page, DocumentParser &parser,
                      const ObjectReference &reference) const {
    page.media_box = parser.resolve_object_copy(media_box);
    if (!page.media_box.is_array()) {
      ODR_WARNING(parser.logger(),
                  "pdf: page " << reference
                               << " has no /MediaBox, defaulting to US Letter");
      page.media_box = default_media_box();
    }

    page.crop_box = parser.resolve_object_copy(crop_box);
    if (!page.crop_box.is_array()) {
      page.crop_box = page.media_box;
    }

    const Object resolved_rotate = parser.resolve_object_copy(rotate);
    page.rotate = resolved_rotate.is_integer()
                      ? normalize_rotate(resolved_rotate.as_integer())
                      : 0;

    if (resources.is_null()) {
      ODR_WARNING(parser.logger(),
                  "pdf: page "
                      << reference
                      << " has no /Resources, using an empty dictionary");
      return Object(Dictionary());
    }
    return resources;
  }
};

/// Parse a simple-font `/Encoding`: either a base-encoding name, or a
/// dictionary with an optional `/BaseEncoding` name overlaid with a
/// `/Differences` array (`code name name … code name …`). Returns `nullopt` for
/// an encoding that cannot be represented (e.g. an unsupported base name with
/// no differences).
std::optional<Encoding> parse_encoding(DocumentParser &parser,
                                       const Object &encoding_object) {
  const Object resolved = parser.resolve_object_copy(encoding_object);

  if (resolved.is_name()) {
    if (const auto base = base_encoding_from_name(resolved.as_name())) {
      return Encoding(*base);
    }
    ODR_WARNING(parser.logger(),
                "pdf: unsupported /Encoding name " << resolved.as_name());
    return std::nullopt;
  }

  if (!resolved.is_dictionary()) {
    return std::nullopt;
  }

  const Dictionary &dictionary = resolved.as_dictionary();

  // No `/BaseEncoding` means "the font's built-in encoding"; reading that from
  // the font program is not wired here. Default to StandardEncoding, the right
  // base for the non-symbolic Latin fonts this path targets.
  auto base = BaseEncoding::standard;
  if (dictionary.has_key("BaseEncoding")) {
    const Object &base_object = dictionary["BaseEncoding"];
    if (base_object.is_name()) {
      if (const auto named = base_encoding_from_name(base_object.as_name())) {
        base = *named;
      } else {
        // An explicitly named base we cannot represent: bail out rather than
        // silently translating every byte through the wrong (Standard) table.
        ODR_WARNING(parser.logger(),
                    "pdf: unsupported /BaseEncoding " << base_object.as_name());
        return std::nullopt;
      }
    }
  }

  Encoding encoding(base);

  if (dictionary.has_key("Differences")) {
    const Object differences =
        parser.resolve_object_copy(dictionary["Differences"]);
    if (differences.is_array()) {
      std::uint32_t code = 0;
      for (const Object &item : differences.as_array()) {
        if (item.is_integer()) {
          code = static_cast<std::uint32_t>(item.as_integer());
        } else if (item.is_name() && code <= 0xFF) {
          encoding.set_difference(static_cast<std::uint8_t>(code),
                                  item.as_name());
          ++code;
        }
      }
    }
  }

  return encoding;
}

/// Parse a CIDFont's `/W` array into the `cid -> width` map. Entries are either
/// `c [w1 w2 ...]` (CIDs c, c+1, ... get the listed widths) or `c_first c_last
/// w` (the whole CID range gets `w`) — ISO 32000-1 9.7.4.3.
void parse_cid_widths(const Array &w, Font &font) {
  // guard against a pathological `c_first c_last` range exhausting memory
  static constexpr std::uint32_t max_range = 70000;

  std::size_t i = 0;
  while (i < w.size()) {
    if (!w[i].is_integer()) {
      ++i;
      continue;
    }
    const auto first = static_cast<std::uint32_t>(w[i].as_integer());
    if (i + 1 < w.size() && w[i + 1].is_array()) {
      const Array &list = w[i + 1].as_array();
      for (std::size_t j = 0; j < list.size(); ++j) {
        if (list[j].is_real()) {
          font.cid_widths[first + static_cast<std::uint32_t>(j)] =
              list[j].as_real();
        }
      }
      i += 2;
    } else if (i + 2 < w.size() && w[i + 1].is_integer() &&
               w[i + 2].is_real()) {
      const auto last = static_cast<std::uint32_t>(w[i + 1].as_integer());
      const double width = w[i + 2].as_real();
      if (last >= first && last - first < max_range) {
        for (std::uint32_t c = first; c <= last; ++c) {
          font.cid_widths[c] = width;
        }
      }
      i += 3;
    } else {
      ++i;
    }
  }
}

/// The `/Matrix` of a form XObject: a 6-element number array `[a b c d e f]`,
/// defaulting to identity when absent or malformed.
util::math::Transform2D parse_matrix(DocumentParser &parser, Object object) {
  parser.resolve_object(object);
  if (!object.is_array()) {
    return {};
  }
  const Array &array = object.as_array();
  if (array.size() != 6) {
    return {};
  }
  return {array[0].as_real(), array[1].as_real(), array[2].as_real(),
          array[3].as_real(), array[4].as_real(), array[5].as_real()};
}

/// Load the embedded font from a `/FontDescriptor` through the `abstract::Font`
/// interface: `/FontFile2` (TrueType / `CIDFontType2`) -> `SfntFont`, and
/// `/FontFile3` (CFF / `Type1C` / `CIDFontType0C`, or OpenType-CFF) -> either
/// an `SfntFont` (when the program is already a full SFNT, `/Subtype
/// /OpenType`) or a bare `CffFont`. `/FontFile` (Type1) is translated to a CFF
/// (`type1::to_cff`) and read as a `CffFont`, so it reuses the whole CFF path.
/// A malformed font is logged and leaves `font.embedded_font` null, so such
/// fonts keep rendering through the fallback path.
void load_embedded_font(DocumentParser &parser, const Dictionary &descriptor,
                        Font &font) {
  // Capture `/Ascent` (glyph space, /1000) for baseline placement. Both the
  // simple- and composite-font paths route their descriptor through here, so
  // this is the one place that sees every descriptor.
  if (descriptor.has_key("Ascent")) {
    const Object ascent = parser.resolve_object_copy(descriptor["Ascent"]);
    if (ascent.is_real()) {
      font.descriptor_ascent = ascent.as_real() / 1000.0;
    }
  }
  try {
    if (descriptor.has_key("FontFile2") &&
        descriptor["FontFile2"].is_reference()) {
      std::string data =
          parser.read_decoded_stream(descriptor["FontFile2"].as_reference());
      font.embedded_font =
          std::make_shared<font::sfnt::SfntFont>(std::move(data));
    } else if (descriptor.has_key("FontFile3") &&
               descriptor["FontFile3"].is_reference()) {
      std::string data =
          parser.read_decoded_stream(descriptor["FontFile3"].as_reference());
      // The program may be a full SFNT (`/Subtype /OpenType`) or a bare CFF
      // (`Type1C` / `CIDFontType0C`); dispatch on the magic.
      if (font::sfnt::SfntFont::is_sfnt(data)) {
        font.embedded_font =
            std::make_shared<font::sfnt::SfntFont>(std::move(data));
      } else {
        font.embedded_font =
            std::make_shared<font::cff::CffFont>(std::move(data));
      }
    } else if (descriptor.has_key("FontFile") &&
               descriptor["FontFile"].is_reference()) {
      // Type1 (`/FontFile`): translate the font to a CFF, then read it as a
      // CffFont so the whole CFF path (re-encode / wrap / reverse map) applies.
      const std::string data =
          parser.read_decoded_stream(descriptor["FontFile"].as_reference());
      const font::type1::Type1Font type1_font(data);
      font.embedded_font =
          std::make_shared<font::cff::CffFont>(font::type1::to_cff(type1_font));
    }
  } catch (const std::exception &e) {
    ODR_WARNING(parser.logger(),
                "pdf: failed to read embedded font: " << e.what());
  }
}

/// Parse a simple font's `/FirstChar`, `/Widths` and `/FontDescriptor`
/// `/MissingWidth` glyph metrics (ISO 32000-1 9.2.4).
void parse_simple_font_widths(DocumentParser &parser,
                              const Dictionary &dictionary, Font &font) {
  if (dictionary.has_key("FirstChar")) {
    const Object first = parser.resolve_object_copy(dictionary["FirstChar"]);
    if (first.is_integer()) {
      font.first_char = static_cast<int>(first.as_integer());
    }
  }
  if (dictionary.has_key("Widths")) {
    const Object widths = parser.resolve_object_copy(dictionary["Widths"]);
    if (widths.is_array()) {
      for (const Object &width : widths.as_array()) {
        font.widths.push_back(width.is_real() ? width.as_real() : 0.0);
      }
    }
  }
  if (dictionary.has_key("FontDescriptor")) {
    const Object descriptor =
        parser.resolve_object_copy(dictionary["FontDescriptor"]);
    if (descriptor.is_dictionary()) {
      const Dictionary &descriptor_dictionary = descriptor.as_dictionary();
      if (descriptor_dictionary.has_key("MissingWidth")) {
        const Object missing =
            parser.resolve_object_copy(descriptor_dictionary["MissingWidth"]);
        if (missing.is_real()) {
          font.missing_width = missing.as_real();
        }
      }
      load_embedded_font(parser, descriptor_dictionary, font);
    }
  }
}

/// Parse a composite CIDFont's `/CIDToGIDMap` (ISO 32000-1 9.7.4.3): a stream
/// of big-endian `uint16` GIDs indexed by CID, or the name `Identity`. Leaves
/// `font.cid_to_gid` empty for `Identity` (so `glyph_for_code` uses `GID =
/// CID`).
void parse_cid_to_gid_map(DocumentParser &parser, const Object &map,
                          Font &font) {
  if (!map.is_reference()) {
    return; // name `/Identity` (or absent): Identity mapping
  }
  try {
    const std::string data = parser.read_decoded_stream(map.as_reference());
    font.cid_to_gid.reserve(data.size() / 2);
    for (std::size_t i = 0; i + 1 < data.size(); i += 2) {
      font.cid_to_gid.push_back(static_cast<std::uint16_t>(
          (static_cast<unsigned char>(data[i]) << 8) |
          static_cast<unsigned char>(data[i + 1])));
    }
  } catch (const std::exception &e) {
    ODR_WARNING(parser.logger(),
                "pdf: failed to read /CIDToGIDMap: " << e.what());
  }
}

/// Parse a composite (Type0) font's descendant CIDFont (`/DescendantFonts` is a
/// one-element array of the CIDFont): records the `/CIDSystemInfo`
/// `/Registry`/`/Ordering` used to pick a predefined CID -> Unicode table.
/// The Type0 `/Encoding` (code -> CID) is `Identity-H/V` or a predefined CJK
/// CMap; only `/ToUnicode` is used for extraction.
void parse_composite_font(DocumentParser &parser, const Dictionary &dictionary,
                          Font &font) {
  font.composite = true;

  // The Type0 `/Encoding`: a predefined CMap name (`Identity-H`,
  // `UniGB-UCS2-H`, …) or an embedded CMap stream. Record the name; the stream
  // case is left empty (deferred). Drives the predefined Unicode-CMap path in
  // `Font::to_unicode`.
  if (dictionary.has_key("Encoding")) {
    const Object encoding = parser.resolve_object_copy(dictionary["Encoding"]);
    if (encoding.is_name()) {
      font.cid_encoding_name = encoding.as_name();
    }
  }

  if (!dictionary.has_key("DescendantFonts")) {
    return;
  }
  const Object descendants =
      parser.resolve_object_copy(dictionary["DescendantFonts"]);
  if (!descendants.is_array() || descendants.as_array().size() == 0) {
    return;
  }

  const Object cid_font = parser.resolve_object_copy(descendants.as_array()[0]);
  if (!cid_font.is_dictionary()) {
    return;
  }
  const Dictionary &cid_font_dictionary = cid_font.as_dictionary();

  if (cid_font_dictionary.has_key("DW")) {
    const Object dw = parser.resolve_object_copy(cid_font_dictionary["DW"]);
    if (dw.is_real()) {
      font.cid_default_width = dw.as_real();
    }
  }
  if (cid_font_dictionary.has_key("W")) {
    const Object w = parser.resolve_object_copy(cid_font_dictionary["W"]);
    if (w.is_array()) {
      parse_cid_widths(w.as_array(), font);
    }
  }

  // The embedded font and CID -> GID mapping live on the descendant CIDFont
  // (the CIDFontType2 case; CIDFontType0/CFF is not yet read).
  if (cid_font_dictionary.has_key("FontDescriptor")) {
    const Object descriptor =
        parser.resolve_object_copy(cid_font_dictionary["FontDescriptor"]);
    if (descriptor.is_dictionary()) {
      load_embedded_font(parser, descriptor.as_dictionary(), font);
    }
  }
  if (cid_font_dictionary.has_key("CIDToGIDMap")) {
    parse_cid_to_gid_map(parser, cid_font_dictionary["CIDToGIDMap"], font);
  }

  if (!cid_font_dictionary.has_key("CIDSystemInfo")) {
    return;
  }

  const Object system_info =
      parser.resolve_object_copy(cid_font_dictionary["CIDSystemInfo"]);
  if (!system_info.is_dictionary()) {
    return;
  }
  const Dictionary &system_info_dictionary = system_info.as_dictionary();
  if (system_info_dictionary.get("Registry").is_string()) {
    font.cid_registry = system_info_dictionary["Registry"].as_string();
  }
  if (system_info_dictionary.get("Ordering").is_string()) {
    font.cid_ordering = system_info_dictionary["Ordering"].as_string();
  }
}

Font *parse_font(State &state, const ObjectReference &reference) {
  // Shared fonts are parsed once; every page referencing the same font object
  // resolves to the one element so the HTML writer inlines it a single time.
  if (Font *cached = state.find_font(reference); cached != nullptr) {
    return cached;
  }

  DocumentParser &parser = state.parser();
  Document &document = state.document();

  Font *font = document.create_element<Font>();
  state.cache_font(reference, font);

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  font->object_reference = reference;
  font->object = Object(dictionary);

  const bool is_type0 = dictionary.get("Subtype").is_name() &&
                        dictionary["Subtype"].as_name() == "Type0";

  if (dictionary.has_key("ToUnicode")) {
    const std::string stream =
        parser.read_decoded_stream(dictionary["ToUnicode"].as_reference());
    util::stream::ViewStream ss(stream);
    CMapParser cmap_parser(ss, parser.logger());
    font->cmap = cmap_parser.parse_cmap();
  }

  if (is_type0) {
    // Composite (Type0) font: the `/Encoding` is a code -> CID CMap, not a
    // simple-font glyph-name encoding, so it must not go through
    // `parse_encoding`. Extraction relies on `/ToUnicode` (parsed above).
    parse_composite_font(parser, dictionary, *font);
  } else {
    parse_simple_font_widths(parser, dictionary, *font);
    if (dictionary.has_key("Encoding")) {
      // Simple-font `/Encoding`: a base-encoding name, or a dictionary with
      // `/BaseEncoding` + `/Differences`. The text-extraction fallback for
      // fonts without a `ToUnicode` CMap.
      font->encoding = parse_encoding(parser, dictionary["Encoding"]);
    }
  }

  return font;
}

Element *parse_page_or_pages(State &state, const ObjectReference &reference,
                             Pages *parent, const PageAttributes &inherited);

// parse_resources and parse_x_object are mutually recursive: a form XObject
// carries its own /Resources, which may list further XObjects (possibly back to
// an enclosing form). The parser's XObject cache breaks such cycles by handing
// back the in-progress element, so the in-memory graph mirrors the file.
Resources *parse_resources(State &state, const Object &object);

XObject *parse_x_object(State &state, const ObjectReference &reference) {
  DocumentParser &parser = state.parser();
  Document &document = state.document();

  // Shared XObjects are parsed once; a cyclic form reference resolves to the
  // existing (possibly still-being-built) element instead of recursing.
  if (XObject *cached = state.find_x_object(reference); cached != nullptr) {
    return cached;
  }

  auto *x_object = document.create_element<XObject>();
  // Register before parsing /Resources so a cycle back to this form resolves
  // here rather than recursing forever.
  state.cache_x_object(reference, x_object);

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  x_object->object_reference = reference;
  x_object->object = Object(dictionary);

  const std::string subtype = dictionary.get("Subtype").is_name()
                                  ? dictionary["Subtype"].as_name()
                                  : "";
  if (subtype == "Image") {
    // Image XObjects carry raster data, not a content stream: recognized but
    // not decoded until stage 4 (and `read_decoded_stream` would throw on the
    // image codec anyway).
    x_object->subtype = XObject::Subtype::image;
    return x_object;
  }
  if (subtype != "Form") {
    // Unknown subtype: keep the element but leave it inexecutable.
    return x_object;
  }

  x_object->subtype = XObject::Subtype::form;
  if (dictionary.has_value("Matrix")) {
    x_object->matrix = parse_matrix(parser, dictionary["Matrix"]);
  }
  if (dictionary.has_value("BBox")) {
    const Array box = parser.resolve_object_copy(dictionary["BBox"]).as_array();
    if (box.size() == 4) {
      x_object->bbox =
          std::array<double, 4>{box[0].as_real(), box[1].as_real(),
                                box[2].as_real(), box[3].as_real()};
    }
  }
  // Read the content eagerly so text extraction needs no parser handle.
  x_object->content = parser.read_decoded_stream(object);

  if (dictionary.has_key("Resources")) {
    x_object->resources = parse_resources(state, dictionary["Resources"]);
  }

  return x_object;
}

Resources *parse_resources(State &state, const Object &object) {
  DocumentParser &parser = state.parser();
  Document &document = state.document();

  auto *resources = document.create_element<Resources>();

  const Dictionary dictionary =
      parser.resolve_object_copy(object).as_dictionary();

  resources->object = Object(dictionary);

  if (dictionary.has_value("Font")) {
    const Dictionary font_table =
        parser.resolve_object_copy(dictionary["Font"]).as_dictionary();
    for (const auto &[key, value] : font_table) {
      resources->font[key] = parse_font(state, value.as_reference());
    }
  }

  if (dictionary.has_value("XObject")) {
    const Dictionary x_object_table =
        parser.resolve_object_copy(dictionary["XObject"]).as_dictionary();
    for (const auto &[key, value] : x_object_table) {
      resources->x_object[key] = parse_x_object(state, value.as_reference());
    }
  }

  if (dictionary.has_key("Properties") && !dictionary["Properties"].is_null()) {
    // Named property lists for `BDC`; resolved eagerly so text extraction can
    // recover `/ActualText` without a parser handle (cf. form XObjects).
    const Dictionary property_table =
        parser.resolve_object_copy(dictionary["Properties"]).as_dictionary();
    for (const auto &[key, value] : property_table) {
      resources->properties[key] = parser.resolve_object_copy(value);
    }
  }

  return resources;
}

Annotation *parse_annotation(Document &document, const Dictionary &dictionary) {
  auto *annotation = document.create_element<Annotation>();
  annotation->object = Object(dictionary);
  return annotation;
}

Annotation *parse_annotation(const State &state,
                             const ObjectReference &reference) {
  DocumentParser &parser = state.parser();
  Document &document = state.document();

  IndirectObject object = parser.read_object(reference);
  Annotation *annotation =
      parse_annotation(document, object.object.as_dictionary());
  annotation->object_reference = reference;
  return annotation;
}

Page *parse_page(State &state, const ObjectReference &reference, Pages *parent,
                 PageAttributes attributes) {
  DocumentParser &parser = state.parser();
  Document &document = state.document();

  Page *page = document.create_element<Page>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  page->object_reference = reference;
  page->object = Object(dictionary);
  page->parent = parent;

  // the page overlays its own inheritable entries, then the accumulated
  // attributes are resolved into the page with Table-30 defaults (7.7.3.4)
  attributes.overlay(dictionary);
  const Object resources = attributes.resolve_into(*page, parser, reference);
  page->resources = parse_resources(state, resources);

  // /Contents is a content stream or an array of them, supplied directly or
  // through an indirect reference (7.7.3.3). Resolve a reference first so that
  // a reference to an array is expanded into its stream references rather than
  // mistaken for a single stream.
  const Object &contents = dictionary["Contents"];
  const Object resolved_contents = parser.resolve_object_copy(contents);
  if (resolved_contents.is_array()) {
    for (const Object &e : resolved_contents.as_array()) {
      page->contents_reference.push_back(e.as_reference());
    }
  } else if (contents.is_reference()) {
    page->contents_reference = {contents.as_reference()};
  }

  if (dictionary.has_key("Annots")) {
    // TODO why rvalue not working?
    const Array annotations =
        parser.resolve_object_copy(dictionary["Annots"]).as_array();
    for (const Object &annotation : annotations) {
      // entries are usually indirect references, but inline annotation
      // dictionaries are equally valid (12.5.2)
      if (annotation.is_reference()) {
        page->annotations.push_back(
            parse_annotation(state, annotation.as_reference()));
      } else if (annotation.is_dictionary()) {
        page->annotations.push_back(
            parse_annotation(document, annotation.as_dictionary()));
      }
    }
  }

  return page;
}

Pages *parse_pages(State &state, const ObjectReference &reference,
                   PageAttributes attributes) {
  DocumentParser &parser = state.parser();
  Document &document = state.document();

  auto *pages = document.create_element<Pages>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();

  pages->object_reference = reference;
  pages->object = Object(dictionary);
  pages->count = dictionary["Count"].as_integer();

  // this node overlays its own inheritable attributes before recursing
  attributes.overlay(dictionary);

  for (const Object &kid : dictionary["Kids"].as_array()) {
    pages->kids.push_back(
        parse_page_or_pages(state, kid.as_reference(), pages, attributes));
  }

  return pages;
}

Element *parse_page_or_pages(State &state, const ObjectReference &reference,
                             Pages *parent, const PageAttributes &inherited) {
  DocumentParser &parser = state.parser();

  // TODO we are parsing twice
  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const std::string &type = dictionary["Type"].as_string();

  if (type == "Pages") {
    return parse_pages(state, reference, inherited);
  }
  if (type == "Page") {
    return parse_page(state, reference, parent, inherited);
  }

  throw std::runtime_error("unknown element");
}

Catalog *parse_catalog(State &state, const ObjectReference &reference) {
  DocumentParser &parser = state.parser();
  Document &document = state.document();

  auto *catalog = document.create_element<Catalog>();

  IndirectObject object = parser.read_object(reference);
  const Dictionary &dictionary = object.object.as_dictionary();
  const ObjectReference &pages_reference = dictionary["Pages"].as_reference();

  catalog->object_reference = reference;
  catalog->object = Object(dictionary);
  catalog->pages = parse_pages(state, pages_reference, {});

  return catalog;
}

std::unique_ptr<Document> parse_document_impl(DocumentParser &parser,
                                              const Dictionary &trailer) {
  auto document = std::make_unique<Document>();

  State state(parser, *document);

  document->catalog = parse_catalog(state, trailer["Root"].as_reference());

  return document;
}

} // namespace

DocumentParser::DocumentParser(std::unique_ptr<std::istream> in,
                               std::optional<Decryptor> decryptor,
                               const Logger &logger)
    : m_stream(std::move(in)), m_parser(*m_stream), m_logger{&logger} {
  try {
    auto [xref, trailer] = read_trailer_chain();
    m_xref = std::move(xref);
    m_trailer = std::move(trailer);
  } catch (const std::exception &e) {
    ODR_WARNING(*m_logger, "pdf: cross-reference parsing failed ("
                               << e.what()
                               << "), scanning the file to recover");
    recover_xref();
  }

  if (m_trailer.has_key("Encrypt")) {
    // Build an `Authenticator` from the trailer `/Encrypt` and `/ID`
    // (ISO 32000-1 7.6).

    // The `/Encrypt` dictionary's own `/O`,`/U`,… strings are never encrypted
    // (7.6.2), and need no explicit self-skip guard: it is resolved here while
    // `m_decryptor` is still empty (installed only after this block), so
    // `read_object` leaves its strings raw and caches that copy — every later
    // lookup hits the cache and never re-decrypts it.
    Object encrypt = m_trailer["Encrypt"];
    resolve_object(encrypt);
    if (!encrypt.is_dictionary()) {
      throw std::runtime_error("pdf: /Encrypt is not a dictionary");
    }

    // The trailer /ID[0] feeds the R 2-4 key derivation (raw bytes).
    std::string id0;
    if (m_trailer.get("ID").is_array()) {
      const Array &id = m_trailer["ID"].as_array();
      if (id.size() > 0 && id[0].is_string()) {
        id0 = id[0].as_string();
      }
    }

    // Set only after resolving `/Encrypt` above: `read_object` throws on an
    // encrypted-but-unauthenticated read, and that resolution runs before any
    // decryptor exists. `m_is_encrypted` records that the file declares
    // encryption regardless of whether we can authenticate it, so an
    // unsupported handler still reports as encrypted.
    m_is_encrypted = true;
    m_authenticator = Authenticator::create(encrypt.as_dictionary(), id0);
  }

  // Install the decryptor only after read_trailer_chain(): cross-reference
  // streams are read during that walk and must stay un-decrypted (7.5.8.2).
  m_decryptor = std::move(decryptor);
}

std::istream &DocumentParser::in() { return m_parser.in(); }

FileParser &DocumentParser::parser() { return m_parser; }

const Logger &DocumentParser::logger() const { return *m_logger; }

const Xref &DocumentParser::xref() const { return m_xref; }

const Dictionary &DocumentParser::trailer() const { return m_trailer; }

bool DocumentParser::is_encrypted() const { return m_is_encrypted; }

bool DocumentParser::is_authenticated() const {
  return m_is_encrypted && m_decryptor.has_value();
}

const std::optional<Authenticator> &DocumentParser::authenticator() const {
  return m_authenticator;
}

const std::optional<Decryptor> &DocumentParser::decryptor() const {
  return m_decryptor;
}

bool DocumentParser::authenticate(const std::string &password) {
  if (m_decryptor.has_value()) {
    throw std::runtime_error("pdf: document is already authenticated");
  }
  if (!m_authenticator.has_value()) {
    throw std::runtime_error("pdf: document is not encrypted");
  }
  m_decryptor = m_authenticator->authenticate(password);
  return m_decryptor.has_value();
}

const IndirectObject &
DocumentParser::read_object(const ObjectReference &reference) {
  if (const auto it = m_objects.find(reference); it != std::end(m_objects)) {
    return it->second;
  }

  IndirectObject object;
  object.reference = reference;

  const auto entry_it = m_xref.table.find(reference);
  if (entry_it == std::end(m_xref.table)) {
    ODR_WARNING(*m_logger, "pdf: object " << reference
                                          << " not in cross-reference table, "
                                             "treating as null");
  } else if (const Xref::Entry &entry = entry_it->second; entry.is_used()) {
    in().seekg(entry.as_used().position);
    object = parser().read_indirect_object();
    // Decrypt string leaves (7.6.2). The /Encrypt dictionary needs no special
    // case here: it is read and cached during construction before the
    // decryptor is installed, so its un-decrypted /O,/U,… strings are served
    // from the cache and never reach this path.
    if (is_encrypted() && !is_authenticated()) {
      throw UnauthenticatedReadError();
    }
    if (m_decryptor.has_value()) {
      m_decryptor->decrypt_strings(object.object, object.reference);
    }
  } else if (entry.is_compressed()) {
    const auto &[stream_id, index] = entry.as_compressed();
    const ObjectStream &members =
        load_object_stream(ObjectReference(stream_id, 0));
    if (index >= members.size()) {
      throw std::runtime_error("object stream member index out of range");
    }
    if (members[index].id != reference.id) {
      ODR_WARNING(*m_logger, "pdf: object stream "
                                 << stream_id << " member " << index
                                 << " has id " << members[index].id
                                 << ", expected " << reference.id);
    }
    object.object = members[index].object;
  } else {
    ODR_WARNING(*m_logger, "pdf: reference " << reference
                                             << " to freed object, treating "
                                                "as null");
  }

  return m_objects.emplace(reference, std::move(object)).first->second;
}

const ObjectStream &
DocumentParser::load_object_stream(const ObjectReference &reference) {
  if (const auto it = m_object_streams.find(reference);
      it != std::end(m_object_streams)) {
    return it->second;
  }

  const IndirectObject &object = read_object(reference);
  if (!object.has_stream) {
    throw std::runtime_error("object stream " + reference.to_string() +
                             " has no stream data");
  }
  const Dictionary &dictionary = object.object.as_dictionary();

  const std::string data = read_decoded_stream(object);
  const std::uint32_t n = resolve_object_copy(dictionary["N"]).as_integer();
  const std::uint32_t first =
      resolve_object_copy(dictionary["First"]).as_integer();

  util::stream::ViewStream in(data);
  return m_object_streams
      .emplace(reference, FileParser(in).read_object_stream(n, first))
      .first->second;
}

std::string
DocumentParser::read_object_stream(const ObjectReference &reference) {
  return read_object_stream(read_object(reference));
}

std::string DocumentParser::read_object_stream(const IndirectObject &object) {
  Object length = object.object.as_dictionary()["Length"];
  resolve_object(length);
  if (!length.is_integer()) {
    throw std::runtime_error("unknown length property");
  }
  const std::uint32_t size = length.as_integer();

  // a stream object always carries a stream position
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  in().seekg(object.stream_position.value());
  std::string raw = m_parser.read_stream(static_cast<std::int32_t>(size));

  // Decrypt before filter decoding (7.6.2). Cross-reference streams are read
  // during the trailer-chain walk, before the decryptor exists, so they are
  // never decrypted (7.5.8.2); object streams are decrypted here as a whole,
  // leaving their members' plaintext.
  if (is_encrypted() && !is_authenticated()) {
    throw UnauthenticatedReadError();
  }
  if (m_decryptor.has_value()) {
    raw = m_decryptor->decrypt_stream(object.reference, std::move(raw));
  }
  return raw;
}

std::string
DocumentParser::read_decoded_stream(const ObjectReference &reference) {
  return read_decoded_stream(read_object(reference));
}

std::string DocumentParser::read_decoded_stream(const IndirectObject &object) {
  std::string raw = read_object_stream(object);

  const Dictionary &dictionary = object.object.as_dictionary();
  Object filter;
  Object decode_parms;
  if (dictionary.has_key("Filter")) {
    filter = deep_resolve_object_copy(dictionary["Filter"]);
  }
  if (dictionary.has_key("DecodeParms")) {
    decode_parms = deep_resolve_object_copy(dictionary["DecodeParms"]);
  }

  DecodeResult result = decode(filter, decode_parms, std::move(raw));
  if (result.stopped_at_filter.has_value()) {
    throw std::runtime_error("unexpected image filter: " +
                             *result.stopped_at_filter);
  }
  return std::move(result.data);
}

void DocumentParser::resolve_object(Object &object) {
  if (object.is_reference()) {
    object = read_object(object.as_reference()).object;
  }
}

void DocumentParser::deep_resolve_object(Object &object) {
  if (object.is_reference()) {
    object = read_object(object.as_reference()).object;
  } else if (object.is_array()) {
    for (Object &e : object.as_array()) {
      deep_resolve_object(e);
    }
  } else if (object.is_dictionary()) {
    for (Object &v : object.as_dictionary() | std::views::values) {
      deep_resolve_object(v);
    }
  }
}

Object DocumentParser::resolve_object_copy(Object object) {
  resolve_object(object);
  return object;
}

Object DocumentParser::deep_resolve_object_copy(Object object) {
  deep_resolve_object(object);
  return object;
}

std::pair<Xref, Dictionary>
DocumentParser::read_xref_section(const std::uint32_t position) {
  in().clear();
  in().seekg(position);
  parser().parser().skip_whitespace();

  if (!parser().parser().peek_number()) {
    // classic cross-reference table
    Xref xref = parser().read_xref();
    parser().parser().skip_whitespace();
    Trailer trailer = parser().read_trailer();
    return {std::move(xref), std::move(trailer.dictionary)};
  }

  // cross-reference stream (ISO 32000-1 7.5.8); its dictionary doubles as
  // the trailer dictionary
  const IndirectObject object = parser().read_indirect_object();
  const Dictionary &dictionary = object.object.as_dictionary();

  if (!object.has_stream) {
    throw std::runtime_error("cross-reference stream has no stream data");
  }
  if (!dictionary.get("Type").is_name() ||
      dictionary["Type"].as_name() != "XRef") {
    ODR_WARNING(*m_logger, "pdf: cross-reference stream at "
                               << position << " is not marked /Type /XRef");
  }

  // `/Filter`, `/DecodeParms` and `/Length` are required to be direct in
  // cross-reference streams (7.5.8.2), so no reference resolution here.
  std::string data = read_object_stream(object);
  const DecodeResult decoded = decode(
      dictionary.get("Filter"), dictionary.get("DecodeParms"), std::move(data));
  if (decoded.stopped_at_filter.has_value()) {
    throw std::runtime_error("unexpected image filter in cross-reference "
                             "stream: " +
                             *decoded.stopped_at_filter);
  }

  const Array &widths = dictionary["W"].as_array();
  if (widths.size() != 3) {
    throw std::runtime_error(
        "expected three field widths in cross-reference stream /W");
  }
  const std::array<std::uint32_t, 3> field_widths = {
      static_cast<std::uint32_t>(widths[0].as_integer()),
      static_cast<std::uint32_t>(widths[1].as_integer()),
      static_cast<std::uint32_t>(widths[2].as_integer())};

  std::vector<std::pair<std::uint32_t, std::uint32_t>> subsections;
  if (dictionary.has_key("Index")) {
    const Array &index = dictionary["Index"].as_array();
    for (std::size_t i = 0; i + 1 < index.size(); i += 2) {
      subsections.emplace_back(index[i].as_integer(),
                               index[i + 1].as_integer());
    }
  } else {
    subsections.emplace_back(0, dictionary["Size"].as_integer());
  }

  util::stream::ViewStream in(decoded.data);
  Xref xref = FileParser(in).read_xref_stream_table(field_widths, subsections);
  return {std::move(xref), dictionary};
}

std::pair<Xref, Dictionary> DocumentParser::read_trailer_chain() {
  parser().seek_start_xref();
  const StartXref start_xref = parser().read_start_xref();

  Xref result_xref;
  std::optional<Dictionary> result_trailer;
  std::optional<std::uint32_t> position = start_xref.start;
  std::set<std::uint32_t> visited; // guards against `Prev` cycles

  while (position.has_value() && visited.insert(*position).second) {
    auto [xref, trailer_dict] = read_xref_section(*position);

    // hybrid-reference file (7.5.8.4): the `XRefStm` entries fill in what
    // the classic table leaves absent or marks free, before older sections
    // are appended
    if (trailer_dict.has_key("XRefStm")) {
      auto [stream_xref, stream_dict] =
          read_xref_section(trailer_dict["XRefStm"].as_integer());
      xref.merge_hybrid(stream_xref);
    }

    result_xref.append(xref);

    if (trailer_dict.has_key("Prev")) {
      position = trailer_dict["Prev"].as_integer();
    } else {
      position.reset();
    }

    if (!result_trailer.has_value()) {
      result_trailer = std::move(trailer_dict);
    }
  }

  // a valid xref chain always sets the trailer
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  return {std::move(result_xref), std::move(result_trailer).value()};
}

void DocumentParser::recover_xref() {
  // Offsets from the failed attempt may be wrong, so anything cached from it is
  // suspect.
  m_objects.clear();
  m_object_streams.clear();
  m_recovered = true;

  std::tie(m_xref, m_trailer) = parser().recover_xref();

  index_object_streams();

  if (!m_trailer.has_key("Root")) {
    recover_root();
  }
}

void DocumentParser::index_object_streams() {
  // Snapshot the directly recovered objects: reading object streams adds
  // compressed entries, which would invalidate an in-flight iterator.
  std::vector<ObjectReference> candidates;
  for (const auto &[reference, entry] : m_xref.table) {
    if (entry.is_used()) {
      candidates.push_back(reference);
    }
  }

  for (const ObjectReference &reference : candidates) {
    try {
      const IndirectObject &object = read_object(reference);
      if (!object.has_stream || !object.object.is_dictionary()) {
        continue;
      }
      const Dictionary &dictionary = object.object.as_dictionary();
      if (!dictionary.get("Type").is_name() ||
          dictionary["Type"].as_name() != "ObjStm") {
        continue;
      }
      const ObjectStream &members = load_object_stream(reference);
      for (std::size_t i = 0; i < members.size(); ++i) {
        // a directly recovered object wins over its compressed copy
        m_xref.table.try_emplace(ObjectReference(members[i].id, 0),
                                 Xref::Entry(Xref::CompressedEntry{
                                     static_cast<std::uint32_t>(reference.id),
                                     static_cast<std::uint32_t>(i)}));
      }
    } catch (const std::exception &) { // NOLINT(bugprone-empty-catch)
      // an unreadable (or, when encrypted, undecryptable) object stream is
      // simply not indexed
    }
  }
}

void DocumentParser::recover_root() {
  for (const auto &[reference, entry] : m_xref.table) {
    if (entry.is_free()) {
      continue;
    }
    try {
      const IndirectObject &object = read_object(reference);
      if (!object.object.is_dictionary()) {
        continue;
      }
      const Dictionary &dictionary = object.object.as_dictionary();
      if (dictionary.get("Type").is_name() &&
          dictionary["Type"].as_name() == "Catalog") {
        ODR_WARNING(*m_logger, "pdf: recovered document catalog " << reference);
        m_trailer["Root"] = Object(reference);
        return;
      }
    } catch (const std::exception &) { // NOLINT(bugprone-empty-catch)
      // skip objects that fail to read during the catalog search
    }
  }
  ODR_WARNING(*m_logger, "pdf: recovery found no document catalog");
}

std::unique_ptr<Document> DocumentParser::parse_document() {
  try {
    return parse_document_impl(*this, m_trailer);
  } catch (const std::exception &e) {
    // The cross-reference table parsed cleanly but does not describe a usable
    // document (no `/Root`, offsets pointing at the wrong objects, …). Scan the
    // file once and retry; if recovery already ran, give up.
    if (m_recovered) {
      throw;
    }
    ODR_WARNING(*m_logger, "pdf: building the document failed ("
                               << e.what()
                               << "), scanning the file to recover");
    recover_xref();
    return parse_document_impl(*this, m_trailer);
  }
}

} // namespace odr::internal::pdf
