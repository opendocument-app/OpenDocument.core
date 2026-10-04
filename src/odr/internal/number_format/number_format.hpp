#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::number_format {

/// One piece of a section of a format code.
struct Token final {
  enum class Kind {
    literal,  ///< text shown as it is
    digit,    ///< `0`, `#` or `?`, in `placeholder`
    point,    ///< the decimal point
    comma,    ///< grouping or scaling, as its place decides
    percent,  ///< `%`, which also scales by 100
    exponent, ///< `E+` or `E-`; `plus` says which
    slash,    ///< the bar of a fraction
    text,     ///< `@`, the text of a text value
    general,  ///< `General`
  };

  Kind kind{Kind::literal};
  std::string text{};    ///< the literal
  char placeholder{'0'}; ///< the digit placeholder
  bool plus{false};      ///< `E+` rather than `E-`
};

/// `[<op><number>]` ahead of a section.
struct Condition final {
  enum class Operator {
    less,
    less_equal,
    greater,
    greater_equal,
    equal,
    not_equal
  };

  Operator op{Operator::equal};
  double operand{0};

  [[nodiscard]] bool matches(double value) const;
};

struct Section final {
  std::vector<Token> tokens{};
  std::optional<Condition> condition{};
};

/// @brief A number format, parsed from a format code as MS-XLS 2.4.126 states
/// its grammar.
///
/// `.` and `,` are written as the code spells them, whatever the locale. A
/// colour is parsed and not shown, and `*` fills nothing.
class Format final {
public:
  /// `General`.
  Format();
  /// @throws std::invalid_argument where @p code is no format code.
  explicit Format(std::string_view code);

  /// The text a cell holding @p value shows.
  [[nodiscard]] std::string format(double value) const;
  /// The text a cell holding the text @p value shows.
  [[nodiscard]] std::string format(std::string_view value) const;

  [[nodiscard]] const std::vector<Section> &sections() const;

private:
  std::vector<Section> m_sections;
};

/// `General`: up to 15 significant digits, in scientific notation outside
/// [1e-10, 1e15).
[[nodiscard]] std::string format_general(double value);

} // namespace odr::internal::number_format
