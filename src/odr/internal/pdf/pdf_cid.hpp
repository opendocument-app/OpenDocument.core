#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::pdf {

/// Translate a string of character codes through a *predefined* Type0 CMap
/// (a composite font's `/Encoding` named in the PDF, e.g. `UniGB-UCS2-H`),
/// returning the UTF-8 text.
///
/// Supports the predefined **Unicode** CMaps — the
/// `Uni*-UCS2`, `Uni*-UTF16` and `Uni*-UTF32` families — whose character codes
/// already *are* Unicode (big-endian), so they are decoded directly with no
/// data tables. Returns `nullopt` for the legacy CJK code→CID CMaps
/// (RKSJ/EUC/Big5/GBK/KSC) and for `Identity-H/V`, which need CID→Unicode
/// tables (the legacy CMaps, deferred — see
/// `tools/pdf/generate_cid_data.py`) or the embedded font's reverse
/// map; the caller then treats the run as "no Unicode".
[[nodiscard]] std::optional<std::string>
translate_predefined_cmap(std::string_view name, const std::string &codes);

} // namespace odr::internal::pdf
