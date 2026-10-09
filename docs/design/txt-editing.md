# Plain-text editing design

`frontend/text.js` implements the plain-text editor on the shared
[`odr.editing` API](editing.md). A `TextFile` has a string of lines and no
document element tree; `Document::edit` and `Document::save` do not apply.

## Operations

The editor gates `beforeinput` and stores inverse changes for undo. It
attaches to `odr.editing`, exposing the common mode, operation log, undo/redo
and callbacks. The view emits `data-odr-editable` and `data-odr-keyboard`.

The coalesced log contains at most one operation with the complete text:

```json
{"version": 2, "ops": [{"op": "setContent", "text": "…"}]}
```

No line IDs or character offsets cross the bridge. An empty envelope makes
no edit. The whole text crosses the bridge on save.

## C++ API

```cpp
[[nodiscard]] bool TextFile::is_savable() const noexcept;
void TextFile::edit(std::string_view operations,
                    const Logger & = Logger::null()) const;
void TextFile::save(std::ostream &out) const;
```

Edits mutate shared file state and appear through every handle and subsequent
render. `TextFile::write_edited` remains deprecated.

## Encoding

Only `FileType::text_file` is savable, with a decodable or unknown encoding.
JSON and known unsupported encodings are not savable. Saves use UTF-8. Shift-JIS is detected but is
not currently decodable or editable. Query encoding support before offering
editing.

## Open work

- Encoding-preserving saves need `from_utf8` and a policy for unrepresentable
  characters.
- Replacing a selection currently creates two undo steps; combine them.
- PDF annotation remains outside the shared editing mode.
