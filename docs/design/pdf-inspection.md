# PDF inspection design

A caller can walk an opened document through `Document` and `Element`. A pdf
offers nothing like it: `PdfFile` decrypts, reports permissions and appends
annotations, and everything it parses stays in `internal/pdf`. This design
adds a public, read-only view of a pdf: its pages, annotations, fonts and
images, the content of a page, and the raw objects beneath them.

## The handles are not `Document`

`Document` and `Element` describe flowing content: paragraphs, spans, tables,
and edits that a save writes back. A pdf page is glyph runs, paths and images
at fixed positions. Mapping it onto `ElementType` would invent paragraphs the
file does not have, which is a second interpretation of the format. Grouping
runs into words, lines and blocks is the job of the reading-order work
([`READING_ORDER.md`](../../src/odr/internal/pdf/READING_ORDER.md)), not of
this view.

So the pdf gets its own family of handles, built the way `Document` is. Each
handle shares the parsed file, is cheap to copy, and has single inheritance
only, so every binding can mirror it.

## A parsed file to hold

`PdfFile` keeps no parsed state: a parser lives for one render. The view needs
one that lives as long as the caller holds it.

```cpp
class PdfFile final : public DecodedFile {
public:
  /// The parsed file. Parses the cross-reference data and the page tree.
  /// Pages and objects are read when asked.
  /// @throws FileEncryptedError before @ref decrypt for a file with a user
  /// password.
  [[nodiscard]] PdfDocument document(const Logger &logger = Logger::null()) const;
};
```

`PdfDocument` owns the `DocumentParser`, its `Decryptor` and the
`pdf::Document` element arena behind one `shared_ptr`. That is the state the
renderer builds for each render. The parser caches each decoded object stream
it reads, so that memory stays in use for as long as any handle does.

The parser fills its caches as it reads, so a `const` call may change them.
One `PdfDocument` and its handles are used from one thread at a time.

## The three layers

`internal/pdf` has three layers. Each is moved into the public API as-is or
wrapped in a public handle ("shell"), as follows.

### Objects: wrapped

`pdf::Object` keeps its value in `std::any`. It has mutable accessors and is
easy to copy by accident; the pdf `AGENTS.md` lists that as a cost. Making it
public would fix that choice in the API.

```cpp
struct PdfObjectReference final {   // the internal one, at the spec's widths (7.5.4)
  std::uint32_t id{};
  std::uint16_t generation{};
};

enum class PdfObjectType {
  null, boolean, integer, real, string, name, array, dictionary, reference,
};

class PdfObject final {
public:
  [[nodiscard]] PdfObjectType type() const noexcept;

  /// Each throws ValueNotStated unless @ref type matches.
  [[nodiscard]] bool as_boolean() const;
  [[nodiscard]] std::int64_t as_integer() const;
  [[nodiscard]] double as_real() const;            ///< an integer as well
  [[nodiscard]] const std::string &as_string() const;  ///< the bytes, decrypted
  [[nodiscard]] const std::string &as_name() const;
  [[nodiscard]] PdfObjectReference as_reference() const;

  [[nodiscard]] std::size_t size() const;          ///< array or dictionary
  [[nodiscard]] PdfObject at(std::size_t index) const;
  [[nodiscard]] std::vector<std::string> keys() const;
  /// A null object where the dictionary has no @p key.
  [[nodiscard]] PdfObject get(std::string_view key) const;

  /// The object a reference points to; any other object unchanged. A free or
  /// absent reference resolves to null, as it does inside the parser.
  [[nodiscard]] PdfObject resolve() const;
};
```

- A `PdfObject` shares the parsed file, so `resolve` needs no other handle.
- The type of a string is `string`, whether the file wrote it in literal or
  hex form. The form is how the file spelled it, not part of its value.
- A stream is the dictionary that describes it. Reading its decoded bytes is
  open work.

### The page tree: wrapped

`Catalog`, `Page`, `Annotation`, `Font` and `XObject` live in the parser's
element arena. They point at each other with raw pointers and are told apart
by `dynamic_cast`. A public handle is the shared parsed file plus a pointer
into the arena, as an `Element` is the document plus an adapter and an id.
Its identity is its `PdfObjectReference`, which the file fixes.

```cpp
class PdfDocument final {
public:
  [[nodiscard]] std::vector<PdfPage> pages() const;
  [[nodiscard]] PdfObject trailer() const;
  [[nodiscard]] PdfObject catalog() const;
  [[nodiscard]] PdfObject object(PdfObjectReference reference) const;
};

class PdfPage final {
public:
  [[nodiscard]] PdfObjectReference reference() const;
  [[nodiscard]] PdfRect media_box() const;
  [[nodiscard]] PdfRect crop_box() const;
  [[nodiscard]] std::int32_t rotation() const;     ///< 0, 90, 180 or 270
  [[nodiscard]] std::vector<PdfAnnotation> annotations() const;
  [[nodiscard]] PdfObject object() const;
};

class PdfAnnotation final {
public:
  [[nodiscard]] PdfObjectReference reference() const;
  [[nodiscard]] std::string subtype() const;       ///< `Highlight`, `Link`, ...
  [[nodiscard]] PdfRect rect() const;
  [[nodiscard]] std::optional<std::string> contents() const;
  [[nodiscard]] std::optional<Color> color() const;
  [[nodiscard]] PdfObject object() const;
};
```

`PdfFont` and `PdfImage` follow the same pattern: a reference, a few
dedicated getters, and `object()`.

**Every handle has `object()`.** The arena keeps each element's original
dictionary "so an unmodelled key stays inspectable", and `object()` carries
that rule into the public API. A getter of its own is added only when
someone needs that key, so the API does not have to model the whole spec at
once.

**The dedicated getters read what the parser already resolved.** `media_box`
and `rotation` are the inherited values `Page` holds. `subtype` and `rect`
are dictionary lookups. None of them interprets anything the renderer does
not.

### Page content: public copies

`TextElement`, `PathElement`, `ImageElement`, `ShadingElement` and
`GroupElement` are already resolved snapshots in user space, in paint order.
They are the closest of the three layers to public, but they cannot move
unchanged:

- They hold raw `Font *`, `XObject *` and `Shading *` pointers into the arena.
- They use `util::math::Transform2D` and `GraphicsState::Color`, both
  internal.
- Some fields exist only for the HTML renderer: `leading_space_inferred`,
  `render_as_graphics`, and `codes` as the renderer pairs them with advances.

So the public versions are copies with a curated set of fields. They hold a
`PdfFont` or a `PdfImage` handle where the internal struct holds a pointer.

```cpp
struct PdfText final {
  PdfMatrix transform;   ///< text space to user space, without the size
  std::optional<PdfFont> font;
  double size{};
  std::string text;      ///< empty where the file maps no Unicode
  double width{};        ///< in text space
  Color fill_color;
  double fill_alpha{1};
};

struct PdfPath final { /* subpaths, fill, stroke, colours, line width, dash */ };
struct PdfImageDraw final { PdfMatrix transform; PdfImage image; double alpha{1}; };

using PdfContentItem = std::variant<PdfText, PdfPath, PdfImageDraw, ...>;

class PdfPage final {
public:
  /// Extracted each time it is asked, as one render extracts it.
  [[nodiscard]] std::vector<PdfContentItem> content() const;
};
```

The exact field list of each struct is settled in its own PR. The rule is
that a field is public only if a caller can use it without knowing how the
renderer draws.

## New value types

- `PdfRect` (`x1`, `y1`, `x2`, `y2`) and `PdfMatrix` (`a` to `f`, as the spec
  names them). No public type covers either yet. They are plain value types
  with the pdf prefix: their meaning is the spec's, in user space, so a
  general geometry type would claim more than they mean.
- Colours are the public `Color`, after the conversion the renderer does
  (CMYK as Adobe converts it). The colour space a file states stays
  internal. A caller that needs it reads it through `object()`.

## Not a second interpretation

Every handle reads the structs the HTML renderer reads. Nothing is parsed,
resolved or extracted a second time, and the renderer keeps using the
internal types directly. When the parser learns something new, the view gets
it through the same structs.

## Kept internal

- The content-stream operators. `PdfPage::content()` is the operators
  already executed.
- The encryption key and the `Decryptor`.
- Grouping runs into words, lines or blocks. When the reading-order work
  lands, it can add a `PdfPage::text()`.

## Plan

Each step is one PR, with a CHANGELOG entry and tests that build their pdfs
inline.

1. **`PdfDocument` and objects.** `PdfFile::document()`, `PdfObject`,
   `PdfObjectReference`, `trailer()`, `catalog()` and `object()`. The parse
   state moves out of the render into a shared owner, and the renderer builds
   it the same way it does now.
2. **Pages and annotations.** `PdfPage`, `PdfRect`, `PdfAnnotation`. This is
   enough for an app to list the highlights a file already has.
3. **Fonts and images.** `PdfFont` (name, subtype, embedded) and `PdfImage`
   (width, height, the browser-ready bytes and their mime type).
4. **Page content.** `PdfMatrix`, `PdfText`, `PdfPath`, `PdfImageDraw` and the
   rest, and `PdfPage::content()`.
5. **Bindings.** pyodr, Java, Swift and npm, after the core API has settled.
   The handles are flat, so each mirrors them one to one.

A corpus test runs steps 1 to 4 over every pdf in `test/data/input`: each
opens, every page lists its annotations and content, and the page count
equals the one the renderer finds.

## Open work

- The decoded bytes of a stream object.
- The document outline (`/Outlines`) and XMP metadata. The parser reads
  neither yet; both are on the pdf roadmap.
- `PdfPage::text()`, once the reading-order work lands.
