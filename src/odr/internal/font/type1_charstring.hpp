#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font::type1 {

/// The result of translating a Type1 charstring to Type2 (CFF).
struct Type2Charstring {
  std::string charstring; ///< the Type2 charstring (no leading width)
  std::int32_t width{};   ///< advance width from `hsbw`/`sbw`, in glyph units
  bool has_width{};       ///< whether an `hsbw`/`sbw` set the width
};

/// Translate one **decrypted** Type1 charstring to a Type2 (CFF) charstring.
///
/// Type1 and Type2 share most path operators; this flattens `callsubr`
/// (inlining @p subrs), folds `div`, lifts the `hsbw` side bearing into the
/// first move, drops Type1-only hint operators (`dotsection`, `*stem3`, hint
/// replacement) and translates the flex and `seac` OtherSubr mechanisms. The
/// advance width (`hsbw`) is returned separately rather than baked into the
/// charstring, so the caller emits it against the CFF `nominalWidthX`.
///
/// Best-effort and display-oriented: hints are dropped (they affect rendering
/// quality, not glyph shape), and unknown operators are skipped.
[[nodiscard]] Type2Charstring to_type2(std::string_view type1,
                                       const std::vector<std::string> &subrs);

} // namespace odr::internal::font::type1
