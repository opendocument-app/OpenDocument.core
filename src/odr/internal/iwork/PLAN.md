# iWork plan

Open work for `iwork/`. What has landed and the rules it set are in
[`AGENTS.md`](AGENTS.md). Every item here stays behind the fail-soft rule and
the `Budget`, and a byte layout counts as fact only once a fixture in the repo
pins it.

## Styles

- `Index/DocumentStylesheet.iwa`. Style archives are sparse property sets with
  a parent reference, so resolution is an inheritance walk that ends at the
  theme's default. Cache resolved styles by identifier, and let equal property
  sets share one resolved style as `.doc` does.
- Character and paragraph properties map to `TextStyle` and `ParagraphStyle`.
  Adjacent runs with equal styles merge into one span.
- Font names are interned in the style registry and never mutated afterwards,
  so `TextStyle::font_name` (`const char *`) stays valid. This is the `.doc`
  and `.xls` rule.
- Page geometry goes to `TextRootAdapter::text_root_page_layout`, which is
  empty today.
- `iwork_style.cpp` is the file for `TSS` property-set inheritance.

## Drawables, images, frames

- A drawable archive carries a geometry (position, size, transform) and a
  content reference. It becomes a `Frame` naming its `ShapeType`, plus `Image`
  where it maps. `iwork_drawable.cpp` is the file for `TSD` geometry, shapes
  and images.
- `Data/` needs no decoder. Media is stored as ordinary zip entries. Open one
  through the filesystem and hand it to `ImageAdapter::image_file` with
  `image_is_internal() == true`.
- Keynote reads the geometry of its text boxes already. What is still owed is
  images, shapes, and the anchoring a Pages text flow does. `parse_attachment`
  in `iwork_text.cpp` skips every anchored object that is not a table.
- Numbers skips every sheet drawable that is not a table. `sheet_first_shape`
  in `iwork_document.cpp` is where they go.
- Pages page-layout mode is drawables on pages with no body flow. Once frames
  exist it is a different root assembly, not new parsing. It keeps the same
  `FileType` and `DocumentType::text`.
- Keynote masters (`Index/TemplateSlide-*.iwa`) give `slide_master_page`, which
  is null today.

## Deferred, by decision

- iWork '05 to '09 (Pages 1 to 4, Keynote 1 to 5, Numbers 1 to 2). Gzipped
  XML, `index.xml.gz` for Pages and Numbers and `index.apxl(.gz)` for Keynote.
  A different format that needs its own engine, for files no application has
  written since 2013. If it ever happens it lives at `iwork/legacy/` behind
  the same three `FileType`s, which is why those are named for the app and
  not the era.
- Password-protected files: `Index/Metadata.iwph` and encrypted `.iwa`s.
  Nothing here has seen one. `password_encrypted()` is not answered, and an
  encrypted package is reported as a zip.
- The package form. On macOS a `.pages` can be a directory. The engine would
  not notice, because `common::SystemFilesystem` roots at a directory, but
  nothing in the public API opens a directory today. A seam, not a stage.
- Templates and iBooks Author (`.template`, `.kth`, `.nmbtemplate`, `.iba`).
  Same container. Add the extensions to the existing rows.
- Charts, comments, footnotes, change tracking, formulas
  (`CalculationEngine.iwa`), number formats, merged cell ranges,
  `ViewState.iwa`, Keynote builds and presenter notes.
- Writing and editing.
- A preview fallback. Every iWork file embeds a rendered preview
  (`preview.jpg`, `preview-web.jpg`, `preview-micro.jpg`). odf and ooxml carry
  thumbnails too, so rendering a preview in place of a decode is a
  library-wide mechanism with its own capability story, not an iWork feature.
