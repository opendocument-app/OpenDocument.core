# `tools/formula`

Generators for the formula engine (`src/odr/internal/formula/`). They are not
part of the build. Each writes committed C++ source from third-party data, so
the build has no dependency on Python. See
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the provenance and the
terms of the data.

The script downloads its input from a pinned commit into a git-ignored cache
next to the script on first run, and reuses it afterwards. To refresh the data,
bump the pin in the script, delete the cache, and rerun.

| Script | Output | Input | Cache |
|---|---|---|---|
| `generate_function_names.py` | `formula_function_names.cpp`: the names LibreOffice shows for each function the evaluator implements, in the languages of `number_format::boolean_words` | `source/<language>/formula/messages.po` of the [LibreOffice translations](https://github.com/LibreOffice/translations) (`_COMMIT`) | `po/` |

```bash
python3 tools/formula/generate_function_names.py
```
