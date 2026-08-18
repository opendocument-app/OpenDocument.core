#pragma once

#include <odr/internal/pdf/pdf_afm.hpp>
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
struct SoftMaskDef;

struct Element {
  virtual ~Element() = default;

  ObjectReference object_reference;
  Object object;

  /// RTTI-backed discrimination mirroring `Object`'s `is_*`/`as_*` surface.
  /// `T` must be complete at the call site.
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

struct Annotation final : Element {
  /// The normal appearance to paint (`/AP /N`, 12.5.5), resolved through
  /// `/AS` when `/N` is a dictionary of states. Null when there is none, the
  /// annotation is hidden, or the appearance is not a form.
  XObject *appearance{nullptr};
  /// Places `appearance`'s `/Matrix`-transformed `/BBox` onto `/Rect`; the
  /// form's own `/Matrix` concatenates onto this, as it would at `Do`.
  util::math::Transform2D appearance_transform;
  /// `/CA` (12.5.2), the opacity the whole appearance composites at.
  double appearance_alpha{1};
};

/// A resource dictionary (ISO 32000-1 7.8.3). Every subdictionary is resolved
/// eagerly at parse time so extraction needs no parser handle. Element pointers
/// are non-owning (the `Document` arena owns them); the plain value types
/// (`ColorSpaceDef`, `Shading`, `SoftMaskDef`) are shared because a single one
/// may be reached from several resources.
struct Resources final : Element {
  std::unordered_map<std::string, Font *> font;
  std::unordered_map<std::string, XObject *> x_object;
  /// `/ColorSpace` (8.6.3), for `cs`/`CS`. Device spaces are not stored — they
  /// resolve by name at use time.
  std::unordered_map<std::string, std::shared_ptr<ColorSpaceDef>> color_space;
  /// `/Properties` (7.8.3): the property lists `BDC` names, for `/ActualText`.
  std::unordered_map<std::string, Object> properties;
  /// `/Shading` (8.7.4.3), for `sh`; the tint function already sampled.
  std::unordered_map<std::string, std::shared_ptr<Shading>> shading;
  /// `/Pattern` (8.7.3.3), for `scn`/`SCN` in a `/Pattern` colour space.
  std::unordered_map<std::string, Pattern *> pattern;
  /// `/ExtGState` (8.4.5), for `gs`; kept verbatim and interpreted at `gs`
  /// time (the extractor reads `ca`/`CA` and `/BM`).
  std::unordered_map<std::string, Object> ext_g_state;
  /// The `/SMask` of each `/ExtGState` that carries one, keyed alike. Absent
  /// for `/SMask /None` too, so the `gs` handler consults `ext_g_state` to tell
  /// "no mask" from "clear the mask" apart.
  std::unordered_map<std::string, std::shared_ptr<SoftMaskDef>>
      ext_g_state_soft_mask;
};

/// An unrendered soft mask (`/ExtGState` `/SMask`, ISO 32000-1 11.6.5.2): the
/// page extractor runs `/G` to produce the `SoftMask` in
/// `pdf_page_element.hpp`.
struct SoftMaskDef {
  enum class Type { luminosity, alpha };
  Type type{Type::luminosity};
  /// The `/G` transparency-group form XObject; never null for a stored def.
  XObject *group{nullptr};
  /// `/BC` backdrop components in the group's colour space; empty = default.
  std::vector<double> backdrop;
};

/// Type3 font glyph data (ISO 32000-1 9.6.5): glyphs are content streams, and
/// glyph space is whatever `/FontMatrix` says, not the fixed 1/1000 em.
struct Type3Data {
  util::math::Transform2D font_matrix; ///< glyph space -> text space
  /// `/CharProcs`: glyph name -> decoded char-proc content stream.
  std::unordered_map<std::string, std::string> char_procs;
  /// `/Resources`, or null to inherit the invoking page's.
  Resources *resources{nullptr};
};

/// An external object invoked by `Do` (ISO 32000-1 8.8): a reusable content
/// stream (form) or raster data (image).
struct XObject final : Element {
  enum class Subtype {
    unknown, ///< neither `/Form` nor `/Image`
    form,    ///< `/Form`: an executable content stream
    image,   ///< `/Image`: raster data
  };

  Subtype subtype{Subtype::unknown};

  // --- form (`/Subtype /Form`) ---
  util::math::Transform2D matrix; ///< `/Matrix`, concatenated at `Do` (8.10.1)
  /// `/BBox` in form space, clipping the content (8.10.2). The spec requires
  /// it; `nullopt` is a lenience for files that omit it.
  std::optional<std::array<double, 4>> bbox;
  /// `/Resources`, or null to inherit the invoking scope's (7.8.3).
  Resources *resources{nullptr};
  /// `/Group` with `/S /Transparency` (11.6.6). Such a group composites in
  /// isolation, so the alpha/mask in force at `Do` applies to its result as a
  /// whole — even when its own content resets them.
  bool transparency_group{false};
  std::string content; ///< decoded content stream, read eagerly

  // --- image (`/Subtype /Image`) ---
  /// Browser-ready bytes: a `DCTDecode` JPEG passed through, or a raster
  /// re-encoded as PNG (with any `/SMask`/`/Mask` composited into RGBA). Empty
  /// for an undecodable codec (CCITT/JBIG2) and for a stencil, so `Do` skips
  /// it.
  std::string image_data;
  std::string image_mime;

  /// `/ImageMask true` (8.9.6.2): a 1-bpc stencil painted in the current fill
  /// colour, which is known only at `Do` time — so the bitmap is carried raw
  /// for the extractor to recolour and `image_data` stays empty.
  bool stencil_mask{false};
  std::string stencil_samples;        ///< 1-bpc bitmap, rows byte-aligned
  std::int32_t stencil_width{0};      ///< `/Width`
  std::int32_t stencil_height{0};     ///< `/Height`
  std::vector<double> stencil_decode; ///< `/Decode`, empty = default `[0 1]`
};

/// A `/Pattern` resource (ISO 32000-1 8.7.3), selected as a colour by
/// `scn`/`SCN` in a `/Pattern` colour space.
struct Pattern final : Element {
  enum class Type {
    unknown,
    tiling,  ///< `/PatternType 1`
    shading, ///< `/PatternType 2`
  };
  Type type{Type::unknown};

  /// `/Matrix`: pattern space -> the parent content stream's default space
  /// (8.7.3.1) — *not* the CTM at use time.
  util::math::Transform2D matrix;

  /// Shading pattern: the gradient painted through the path, pre-resolved.
  std::shared_ptr<Shading> shading;

  /// Tiling pattern: a `/BBox` cell repeated every `/XStep`/`/YStep`. An
  /// uncoloured cell (`/PaintType 2`) takes the fill colour at use time.
  std::int32_t paint_type{0};    ///< `/PaintType`: 1 coloured, 2 uncoloured
  std::array<double, 4> bbox{};  ///< `/BBox` `[x0 y0 x1 y1]`, pattern space
  double x_step{0};              ///< `/XStep`, pattern space
  double y_step{0};              ///< `/YStep`, pattern space
  Resources *resources{nullptr}; ///< the tile's own `/Resources`
  std::string content;           ///< decoded tile content stream
};

/// A non-owning view over a string of PDF character codes, splitting it into
/// big-endian codes on iteration; must not outlive the bytes. A trailing
/// partial code is dropped, as the text-showing operators do.
///
/// A `codespace` CMap picks each code's width from its first byte (ISO 32000-1
/// 9.7.6.2), so a mixed 1-/2-byte encoding (a 1-byte space among 2-byte CIDs,
/// which PDFClown et al. emit) stays aligned; without one the width is a fixed
/// `Font::code_byte_width()`.
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
    Iterator(const std::string_view range, const std::size_t width,
             const CMap *codespace, const CMap *cid_map)
        : m_range{range}, m_fixed_width{width}, m_codespace{codespace},
          m_cid_map{cid_map} {
      settle();
    }

    std::uint32_t operator*() const {
      // Yields the CID, which is what the glyph/advance paths expect; without a
      // CID map it is the code itself (`Identity-H/V`, and simple fonts).
      if (m_cid_map != nullptr) {
        if (const std::optional<std::uint32_t> cid =
                m_cid_map->cid_for_code(m_range.substr(0, m_width));
            cid.has_value()) {
          return *cid;
        }
      }
      std::uint32_t code = 0;
      for (std::size_t k = 0; k < m_width; ++k) {
        code = (code << 8) | static_cast<unsigned char>(m_range[k]);
      }
      return code;
    }

    Iterator &operator++() {
      m_range.remove_prefix(m_width);
      settle();
      return *this;
    }
    Iterator operator++(int) {
      Iterator copy = *this;
      ++*this;
      return copy;
    }

    bool operator==(const Iterator &other) const {
      return m_range.data() == other.m_range.data();
    }

  private:
    /// Fix the width of the code at the front of `m_range`. A trailing partial
    /// code is dropped by consuming the rest of the range, so the iterator
    /// lands exactly on the end sentinel.
    void settle() {
      if (m_range.empty()) {
        m_width = 0;
        return;
      }
      std::size_t width = m_fixed_width;
      if (m_codespace != nullptr && m_codespace->has_codespace()) {
        width =
            m_codespace->code_width(static_cast<std::uint8_t>(m_range.front()));
      }
      if (width == 0 || width > m_range.size()) {
        m_range.remove_prefix(m_range.size()); // drop the trailing partial code
        m_width = 0;
        return;
      }
      m_width = width;
    }

    std::string_view m_range;
    std::size_t m_fixed_width{1};
    const CMap *m_codespace{nullptr};
    const CMap *m_cid_map{nullptr};
    std::size_t m_width{0};
  };

  CodeRange(const std::string_view codes, const std::size_t width,
            const CMap *codespace, const CMap *cid_map)
      : m_codes{codes}, m_width{width}, m_codespace{codespace},
        m_cid_map{cid_map} {}

  [[nodiscard]] Iterator begin() const {
    return {m_codes, m_width, m_codespace, m_cid_map};
  }
  [[nodiscard]] Iterator end() const {
    // An exhausted `begin()` consumes down to this same one-past-the-end
    // pointer, and `operator==` matches on `data()`.
    return {m_codes.substr(m_codes.size()), m_width, m_codespace, m_cid_map};
  }

private:
  std::string_view m_codes;
  std::size_t m_width{};
  const CMap *m_codespace{nullptr};
  const CMap *m_cid_map{nullptr};
};

struct Font final : Element {
  /// `ToUnicode` CMap, the primary code -> Unicode path when present.
  CMap cmap;
  /// Simple-font `/Encoding` (base + `/Differences`), the text-extraction
  /// fallback used when no `ToUnicode` CMap is present.
  std::optional<Encoding> encoding;

  /// Composite (Type0): multi-byte codes selecting CIDs through the Type0
  /// `/Encoding` CMap. See `to_unicode` for the extraction fallback chain.
  bool composite{false};
  /// The descendant CIDFont's `/CIDSystemInfo` (e.g. `Adobe`/`Japan1`), which
  /// picks the predefined CID -> Unicode table. Empty for simple fonts.
  std::string cid_registry;
  std::string cid_ordering;
  /// The Type0 `/Encoding` when it is a *predefined* CMap name (`Identity-H`,
  /// `UniGB-UCS2-H`, `90ms-RKSJ-H`, …); empty for an embedded CMap stream.
  std::string cid_encoding_name;
  /// The Type0 `/Encoding` when it is an *embedded* CMap stream (code -> CID
  /// plus codespace, ISO 32000-1 9.7.5.3). Its codespace outranks
  /// `/ToUnicode`'s when splitting a code string (see `codes`).
  CMap cid_encoding;

  /// Simple-font metrics (ISO 32000-1 9.2.4): `/Widths` in glyph space (1/1000
  /// em) indexed by `code - first_char`, else `/MissingWidth`.
  int first_char{0};
  std::vector<double> widths;
  double missing_width{0};

  /// Composite-font metrics (9.7.4.3): `/DW` and the `/W` CID -> width map,
  /// glyph space.
  double cid_default_width{1000};
  std::unordered_map<std::uint32_t, double> cid_widths;

  /// FontDescriptor `/Ascent` in em (i.e. /1000). The HTML layer raises each
  /// run by it, since PDF's text origin is the baseline but a CSS box anchors
  /// its top. Absent -> the layer falls back to the embedded bbox, then 0.8.
  std::optional<double> descriptor_ascent;

  /// Bytes per character code, absent a codespace saying otherwise.
  [[nodiscard]] int code_byte_width() const { return composite ? 2 : 1; }

  /// View `codes` as character codes, each yielded as the CID it selects. The
  /// result borrows `codes`.
  ///
  /// The split is by the embedded CID `/Encoding` CMap's codespace if there is
  /// one, else the `/ToUnicode` codespace (which keeps the split identical to
  /// `to_unicode`'s), else the fixed `code_byte_width`.
  [[nodiscard]] CodeRange codes(std::string_view codes) const {
    const auto width = static_cast<std::size_t>(code_byte_width());
    if (!composite) {
      return {codes, width, nullptr, nullptr};
    }
    const CMap *cid_map = cid_encoding.has_cid_map() ? &cid_encoding : nullptr;
    const CMap *codespace = cid_encoding.has_codespace() ? &cid_encoding
                            : cmap.has_codespace()       ? &cmap
                                                         : nullptr;
    return {codes, width, codespace, cid_map};
  }

  /// The embedded font program (`/FontFile`/`2`/`3`, all flavors), or null.
  /// When present the HTML layer re-encodes it to the PUA and renders its real
  /// glyphs via `@font-face`, and `to_unicode` can use its reverse map.
  std::shared_ptr<abstract::Font> embedded_font;

  /// Substitution for a non-embedded simple font: the CSS `font-family` to
  /// render in, plus the standard-14 AFM metrics backing `advance_width` when
  /// the font ships no `/Widths`.
  std::optional<FontSubstitute> substitute;

  /// Set only for `/Subtype /Type3` (ISO 32000-1 9.6.5).
  std::optional<Type3Data> type3;

  /// `/CIDToGIDMap` (9.7.4.3) as an explicit stream: `GID = cid_to_gid[CID]`.
  /// Empty means `Identity`.
  std::vector<std::uint16_t> cid_to_gid;

  /// Glyph id of `code` in `embedded_font`; 0 (`.notdef`) when there is none.
  /// Composite: CID through `/CIDToGIDMap`. Simple TrueType: the embedded
  /// `cmap` keyed on the byte, then on the code's Unicode (9.6.6.4).
  [[nodiscard]] std::uint16_t glyph_for_code(std::uint32_t code) const;

  /// Advance of one code in text-space units (glyph space / 1000); multiply by
  /// the font size for user space.
  [[nodiscard]] double advance_width(std::uint32_t code) const;

  /// Codes -> Unicode, first hit wins: `/ToUnicode`; then for a composite font
  /// the predefined `/Encoding` CMap, the `/CIDSystemInfo` collection, the
  /// embedded reverse map, else nothing; for a simple font the `/Encoding`,
  /// the embedded reverse map, else identity bytes.
  [[nodiscard]] std::string to_unicode(const std::string &codes) const;
};

} // namespace odr::internal::pdf
