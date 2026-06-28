#pragma once

#include <odr/internal/pdf/pdf_cmap.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>
#include <odr/internal/pdf/pdf_object.hpp>
#include <odr/internal/pdf/pdf_shading.hpp>
#include <odr/internal/util/math_util.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::abstract {
class Font;
}

namespace odr::internal::pdf {

struct Pages;
struct Page;
struct Annotation;
struct Resources;
struct Font;
struct XObject;
struct Pattern;
struct ColorSpaceDef;

struct Element {
  virtual ~Element() = default;

  ObjectReference object_reference;
  Object object;

  /// Value-semantic discrimination over the element hierarchy, backed by RTTI;
  /// mirrors `Object`'s `is_*`/`as_*` surface. `T` must be a complete-derived
  /// type at the call site.
  template <typename T>
    requires std::derived_from<T, Element>
  [[nodiscard]] bool is() const {
    return dynamic_cast<const T *>(this) != nullptr;
  }
  template <typename T>
    requires std::derived_from<T, Element>
  [[nodiscard]] T &as() {
    return dynamic_cast<T &>(*this);
  }
  template <typename T>
    requires std::derived_from<T, Element>
  [[nodiscard]] const T &as() const {
    return dynamic_cast<const T &>(*this);
  }
};

struct Catalog final : Element {
  Pages *pages{nullptr};
};

struct Pages final : Element {
  std::vector<Element *> kids;
  std::uint32_t count{0};
};

struct Page final : Element {
  Pages *parent{nullptr};

  Resources *resources{nullptr};
  std::vector<Annotation *> annotations;

  // resolved inheritable attributes (ISO 32000-1 7.7.3.3, Table 30)
  Object media_box;  // rectangle array
  Object crop_box;   // rectangle array (defaults to media_box)
  Integer rotate{0}; // normalized to {0, 90, 180, 270}

  // TODO remove
  std::vector<ObjectReference> contents_reference;
};

struct Annotation final : Element {};

struct Resources final : Element {
  std::unordered_map<std::string, Font *> font;
  std::unordered_map<std::string, XObject *> x_object;
  /// The `/ColorSpace` subdictionary (ISO 32000-1 8.6.3): named colour spaces
  /// referenced by `cs`/`CS`. Resolved eagerly (ICC alternates, Separation tint
  /// transforms, …) so extraction can convert `sc`/`scn` colours to RGB without
  /// a parser handle. The device spaces (`/DeviceRGB`, …) are not stored here —
  /// they resolve by name at use time.
  std::unordered_map<std::string, std::shared_ptr<ColorSpaceDef>> color_space;
  /// The `/Properties` subdictionary (ISO 32000-1 7.8.3): named property lists
  /// referenced by `BDC`. Each value is the resolved property-list dictionary
  /// `Object`; used to recover `/ActualText` for a `BDC /Tag /Name` sequence.
  std::unordered_map<std::string, Object> properties;
  /// The `/Shading` subdictionary (ISO 32000-1 8.7.4.3): named shadings painted
  /// by the `sh` operator. Resolved eagerly (the tint function sampled into
  /// colour stops) so extraction needs no parser handle.
  std::unordered_map<std::string, std::shared_ptr<Shading>> shading;
  /// The `/Pattern` subdictionary (ISO 32000-1 8.7.3.3): named tiling/shading
  /// patterns selected as a colour by `scn`/`SCN` in a `/Pattern` colour space.
  std::unordered_map<std::string, Pattern *> pattern;
};

/// An external object referenced by `Do` and listed in a resource dictionary's
/// `/XObject` subdictionary (ISO 32000-1 7.8.3, 8.8). Form XObjects carry a
/// reusable content stream; image XObjects are recognized but not rendered
/// until stage 4.
struct XObject final : Element {
  enum class Subtype {
    unknown, ///< neither `/Form` nor `/Image`
    form,    ///< `/Form`: an executable content stream
    image,   ///< `/Image`: raster data (stage 4)
  };

  Subtype subtype{Subtype::unknown};

  /// Form XObject only: the `/Matrix` (default identity), concatenated onto the
  /// CTM when the form is invoked (8.10.1).
  util::math::Transform2D matrix;
  /// Form XObject only: the `/BBox` `[x0 y0 x1 y1]` in form space, clipping the
  /// form's content (8.10.2). `nullopt` when the form declares none (lenient;
  /// the spec requires it).
  std::optional<std::array<double, 4>> bbox;
  /// Form XObject only: the form's own `/Resources`, or `nullptr` to inherit
  /// the invoking scope's resources (7.8.3).
  Resources *resources{nullptr};
  /// Form XObject only: the decoded (filter-applied) content stream, read
  /// eagerly at parse time so text extraction needs no parser handle.
  std::string content;

  /// Image XObject only: the encoded image bytes for the browser — a JPEG
  /// passed through (`DCTDecode`) or a raster re-encoded as PNG (Flate/LZW/raw
  /// samples assembled through the colour space, with a `/SMask`/`/Mask`
  /// composited into RGBA) — with `image_mime` naming the format. Empty for a
  /// codec not yet handled (JPX/CCITT/JBIG2), a stencil mask (see below) and
  /// non-image XObjects, so `Do` skips it.
  std::string image_data;
  std::string image_mime;

  /// Image XObject `/ImageMask true` (ISO 32000-1 8.9.6.2): a 1-bpc stencil
  /// painted in the *current fill colour*, which is known only at `Do` time. So
  /// the decoded bitmap and its geometry are carried here for the page
  /// extractor to recolour (`encode_stencil_png`); `image_data` stays empty.
  /// `false` for a normal image.
  bool stencil_mask{false};
  std::string stencil_samples;    ///< decoded 1-bpc bitmap, rows byte-aligned
  std::int32_t stencil_width{0};  ///< `/Width`
  std::int32_t stencil_height{0}; ///< `/Height`
  std::vector<double> stencil_decode; ///< `/Decode`, empty = default `[0 1]`
};

/// A pattern listed in a resource dictionary's `/Pattern` subdictionary
/// (ISO 32000-1 8.7.3), selected as a colour by `scn`/`SCN` in a `/Pattern`
/// colour space. Shading patterns (`/PatternType 2`) paint a gradient through
/// the path; tiling patterns (`/PatternType 1`) repeat a content-stream cell.
struct Pattern final : Element {
  enum class Type {
    unknown,
    tiling,  ///< `/PatternType 1`
    shading, ///< `/PatternType 2`
  };
  Type type{Type::unknown};

  /// `/Matrix` mapping pattern space to the default coordinate system of the
  /// pattern's parent content stream (8.7.3.1); default identity.
  util::math::Transform2D matrix;

  /// Shading pattern (`/PatternType 2`): the shading painted through the path,
  /// pre-resolved (its tint function sampled into stops). Null otherwise.
  std::shared_ptr<Shading> shading;
};

/// A non-owning view over a string of PDF character codes, splitting it into
/// fixed-width (`Font::code_byte_width()`) big-endian codes on iteration. Holds
/// only a `string_view`, so it must not outlive the underlying bytes; iterate
/// it directly (`for (std::uint32_t code : font.codes(...))`). Trailing bytes
/// shorter than a full code are dropped, matching the PDF text-showing
/// operators.
class CodeRange {
public:
  class Iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::uint32_t;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = std::uint32_t;

    Iterator() = default;
    Iterator(const char *position, std::size_t width)
        : m_position{position}, m_width{width} {}

    std::uint32_t operator*() const {
      std::uint32_t code = 0;
      for (std::size_t k = 0; k < m_width; ++k) {
        code = (code << 8) | static_cast<unsigned char>(m_position[k]);
      }
      return code;
    }

    Iterator &operator++() {
      m_position += m_width;
      return *this;
    }
    Iterator operator++(int) {
      Iterator copy = *this;
      ++*this;
      return copy;
    }

    bool operator==(const Iterator &other) const {
      return m_position == other.m_position;
    }

  private:
    const char *m_position{nullptr};
    std::size_t m_width{1};
  };

  CodeRange(std::string_view codes, std::size_t width)
      : m_codes{codes}, m_width{width} {}

  // `data()` is used as a bounded byte range delimited by `end()`, not as a
  // null-terminated string; the suspicious-data-usage check does not apply.
  [[nodiscard]] Iterator begin() const {
    return {m_codes.data(),
            m_width}; // NOLINT(bugprone-suspicious-stringview-data-usage)
  }
  [[nodiscard]] Iterator end() const {
    // round down to a whole number of codes
    return {
        m_codes.data() +
            (m_codes.size() -
             m_codes.size() %
                 m_width), // NOLINT(bugprone-suspicious-stringview-data-usage)
        m_width};
  }

private:
  std::string_view m_codes;
  std::size_t m_width{};
};

struct Font final : Element {
  /// `ToUnicode` CMap, the primary code -> Unicode path when present.
  CMap cmap;
  /// Simple-font `/Encoding` (base + `/Differences`), the text-extraction
  /// fallback used when no `ToUnicode` CMap is present.
  std::optional<Encoding> encoding;

  /// True for composite (Type0) fonts. Their character codes are
  /// multi-byte and select CIDs via the Type0 `/Encoding` CMap; `/ToUnicode` is
  /// the code -> Unicode path. Code -> CID via predefined CJK CMaps and the
  /// CID -> Unicode tables are deferred; an embedded TrueType font also
  /// supplies a reverse map (see `glyph_for_code` / `to_unicode`).
  bool composite{false};
  /// The descendant CIDFont's `/CIDSystemInfo` `/Registry` and `/Ordering`
  /// (e.g. `Adobe` / `Identity` or `Adobe` / `Japan1`). Recorded for the
  /// predefined CID -> Unicode table selection; empty for
  /// simple fonts.
  std::string cid_registry;
  std::string cid_ordering;
  /// The composite font's `/Encoding` when it is a *predefined* CMap name (e.g.
  /// `Identity-H`, `UniGB-UCS2-H`); empty for an embedded CMap stream. Drives
  /// the predefined Unicode-CMap extraction path.
  std::string cid_encoding_name;

  /// Simple-font glyph metrics (ISO 32000-1 9.2.4): `/Widths`, in glyph space
  /// (1/1000 text-space units), indexed by `code - first_char`; codes outside
  /// the range fall back to `missing_width` (`/MissingWidth`).
  int first_char{0};
  std::vector<double> widths;
  double missing_width{0};

  /// Composite-font glyph metrics (9.7.4.3): the `/DW` default width and the
  /// `/W` map CID -> width, both glyph-space units. Codes are interpreted as
  /// CIDs (identity), the common `Identity-H/V` case.
  double cid_default_width{1000};
  std::unordered_map<std::uint32_t, double> cid_widths;

  /// FontDescriptor `/Ascent` in em units (the raw `/Ascent`, glyph space,
  /// divided by 1000), when the descriptor declares it. The baseline-to-top
  /// distance of the font's glyphs; used to place a run by its baseline (PDF's
  /// text origin) rather than its CSS box top. Absent for fonts with no
  /// descriptor `/Ascent` (the HTML layer then falls back to the embedded
  /// font's bounding box, then a constant).
  std::optional<double> descriptor_ascent;

  /// Bytes per character code: 2 for composite (Type0) fonts (the
  /// `Identity-H/V` and common CID case), 1 for simple fonts.
  [[nodiscard]] int code_byte_width() const { return composite ? 2 : 1; }

  /// View `codes` as a sequence of character codes split per
  /// `code_byte_width()`. The result borrows `codes`; do not outlive it.
  [[nodiscard]] CodeRange codes(std::string_view codes) const {
    return {codes, static_cast<std::size_t>(code_byte_width())};
  }

  /// Embedded font: the SFNT parsed from `/FontFile2` (a simple TrueType font,
  /// or a composite `CIDFontType2`), or `nullptr` when nothing is embedded or
  /// the font is an unsupported flavor (`/FontFile3` CFF, `/FontFile` Type1 —
  /// both not yet read). When present, the HTML layer re-encodes it to the PUA
  /// and renders the actual glyphs via `@font-face`, and `to_unicode` reads the
  /// embedded reverse map.
  std::shared_ptr<abstract::Font> embedded_font;

  /// Composite-font `/CIDToGIDMap` (ISO 32000-1 9.7.4.3) when given as an
  /// explicit stream: `GID = cid_to_gid[CID]`. Empty means `Identity`
  /// (`GID = CID`, the common case).
  std::vector<std::uint16_t> cid_to_gid;

  /// Map a single character code (as split by `code_byte_width`) to a glyph id
  /// in the `embedded_font`. For a composite font the code is the CID
  /// (`Identity-H/V`), mapped to a GID via `/CIDToGIDMap`; for a simple
  /// TrueType font the embedded `cmap` (or, failing that, the code's Unicode)
  /// selects the glyph (ISO 32000-1 9.6.6.4). Returns 0 (`.notdef`) when there
  /// is no embedded font or no mapping.
  [[nodiscard]] std::uint16_t glyph_for_code(std::uint32_t code) const;

  /// Glyph advance width of a single character code, in text-space units
  /// (glyph-space / 1000; multiply by the font size for user space). Falls back
  /// to `/MissingWidth` (simple) or `/DW` (composite) for absent codes.
  [[nodiscard]] double advance_width(std::uint32_t code) const;

  /// Translate a string of character codes to Unicode: the `ToUnicode` CMap
  /// when present (authoritative), else, for a composite font, "no Unicode",
  /// else the simple-font `/Encoding`, else identity bytes.
  [[nodiscard]] std::string to_unicode(const std::string &codes) const;
};

} // namespace odr::internal::pdf
