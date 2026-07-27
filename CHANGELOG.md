# Changelog

Notable changes to `odrcore`. Releases before 6.0.0 are documented only in the
[GitHub releases](https://github.com/opendocument-app/OpenDocument.core/releases).

This project follows [semantic versioning](#versioning-and-api-stability) as
described at the bottom of this file.

## Unreleased — 6.0.0

A breaking release that cleans up the public API. Everything below is a
source-incompatible change to `odr::`; see the migration notes per entry.
Entries without a PR reference come from #627.

### Removed

- **Deprecated API and the pdf2htmlEX / wvWare backends** (#622).
- **`DecoderEngine` and the whole decoder-selection dimension.** Since the
  external backends were dropped in #622, the enum had exactly one value and
  could not select anything. Gone: `DecoderEngine`,
  `DecodePreference::with_engine`, `DecodePreference::engine_priority`,
  `list_decoder_engines`, `decoder_engine_by_name`, `decoder_engine_to_string`,
  `DecodedFile::decoder_engine`, `UnknownDecoderEngine` and
  `UnsupportedDecoderEngine`. Also removed from the Python and Java bindings
  (`DecoderEngine.java` is deleted).
  *Migration:* drop the arguments; there is only one decoder.

### Changed

- **`FileMeta` collapsed into a single type** (#626).
- **`Logger` is a value type with a public `ILogger` sink** (#625).
- **`HttpServer::listen` split into `bind` and `listen`** (#624).
- **Shape geometry is typed.** `Frame`, `Rect`, `Line`, `Circle` and
  `CustomShape` return `Measure` instead of `std::string` for
  `x`/`y`/`width`/`height`/`x1`/`y1`/`x2`/`y2`, and `Frame::z_index` returns
  `std::optional<std::int32_t>` instead of a string.
  *Migration:* call `.to_string()` where you previously got a string.
- **`Measure::to_string` keeps 10 significant digits** instead of 4. The old
  precision silently rounded document geometry (a drawing coordinate of
  `-6734.61mm` was emitted as `-6735mm`). Emitted HTML now carries the more
  accurate value; reference outputs change numerically but not structurally.
- **`TextStyle::font_name` is `std::optional<std::string_view>`** instead of
  `const char *`, matching its sibling fields.
  *Migration:* `style.font_name != nullptr` becomes
  `style.font_name.has_value()`.
- **Every exception derives from `odr::Exception`**, so `catch (const
  odr::Exception &)` catches all typed library errors. Note the decoders still
  throw plain `std::runtime_error` for malformed input without a dedicated
  type. The Python bindings mirror this with a base `pyodr.Error`; the Java
  bindings already had `OdrException`.
- **`Color` uses named factories.** `Color(std::uint32_t rgb)` and
  `Color(std::uint32_t argb, bool dummy)` are replaced by `Color::from_rgb` and
  `Color::from_argb`.
- **`html::edit` takes `std::string_view`** instead of `const char *`.
- **`Html::config()` is `const`**, matching `HtmlService::config()`.
- **`Measure` moved from `<odr/style.hpp>` to `<odr/quantity.hpp>`.** Both
  headers still provide it; `style.hpp` includes `quantity.hpp`.

### Fixed

- **`ElementIterator::operator++(int)` advances the iterator.** It was `const`,
  so it could not advance, and it returned the *successor* rather than the
  pre-increment value — meaning `it++` silently violated the forward-iterator
  contract the class advertises.
- **`Element` and `ElementIterator` equality compare the document.** They
  previously compared only the element identifier, so element *n* of one
  document compared equal to element *n* of an unrelated one.
- `FileType::word_perfect` and `FileType::rich_text_format` are documented as
  detection-only: magic recognises them so a caller can report the type, but
  there is no decoder and opening one throws.

## Versioning and API stability

`odrcore` uses semantic versioning.

- The **public API** is everything under `src/odr/*.hpp` — the headers directly
  in the `odr/` directory. Within a major version these stay
  source-compatible; additions are minor releases.
- **`odr::internal`** (`src/odr/internal/**`) is *not* public. These headers are
  installed because parts of the public API hand back `internal::abstract`
  types, but anything in that namespace may change in any release.
- **ABI** is not guaranteed between releases, including patch releases. Link
  against a matching build.
- The **bindings** (`pyodr`, `app.opendocument.core`) track the C++ API and
  break with it.
