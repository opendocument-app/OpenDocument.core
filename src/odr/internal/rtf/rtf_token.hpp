#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace odr::internal::rtf {

/// `{`
struct GroupOpen final {};
/// `}`
struct GroupClose final {};
/// `\` plus ascii letters, with the signed parameter that followed them.
struct ControlWord final {
  std::string name;
  std::optional<std::int32_t> parameter;
};
/// `\` plus one non-letter — `\\`, `\~`, `\*`, … Never `\'`.
struct ControlSymbol final {
  char symbol{};
};
/// The one byte a `\'hh` escape stands for, still in the run's encoding. Its
/// own token because `\ucN` counts it as one character where a text run counts
/// bytes.
struct HexEscape final {
  char byte{};
};
/// A literal run, still in the run's encoding.
struct Text final {
  std::string bytes;
};
/// The payload of a `\binN`, read raw — it may contain braces and backslashes.
struct Binary final {
  std::string bytes;
};
struct End final {};

using Token = std::variant<GroupOpen, GroupClose, ControlWord, ControlSymbol,
                           HexEscape, Text, Binary, End>;

} // namespace odr::internal::rtf
