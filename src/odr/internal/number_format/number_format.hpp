#pragma once

#include <odr/internal/number_format/calendar_names.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace odr::internal::number_format {

/// One piece of a section of a format code.
struct Token final {
  enum class Kind {
    literal,   ///< text shown as it is
    digit,     ///< `0`, `#` or `?`, in `placeholder`
    point,     ///< the decimal point
    comma,     ///< grouping or scaling, as its place decides
    percent,   ///< `%`, which also scales by 100
    exponent,  ///< `E+` or `E-`; `plus` says which
    slash,     ///< the bar of a fraction
    text,      ///< `@`, the text of a text value
    general,   ///< `General`
    date_time, ///< a part of a date or a time, in `unit` and `width`
  };

  Kind kind{Kind::literal};
  std::string text{};    ///< the literal, or how `AM/PM` is spelled
  char placeholder{'0'}; ///< the digit placeholder
  bool plus{false};      ///< `E+` rather than `E-`
  /// `y`, `M` (month), `d`, `h`, `m` (minute), `s`, `f` (a fraction of a
  /// second) or `a` (`AM/PM`, `A/P`).
  char unit{'\0'};
  std::size_t width{0}; ///< how many letters, or digits of a fraction
  bool elapsed{false};  ///< `[h]`, `[m]` or `[s]`: not wrapped at a day
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
  /// The month and day names of the language `[$-407]` states; null where
  /// the section states none, or one the table does not have.
  const CalendarNames *names{nullptr};
};

/// Where a date serial counts from: `workbookPr/@date1904` picks 1904.
enum class Epoch { from_1900, from_1904 };

/// The days from 1899-12-30 to the civil date, negative before it.
[[nodiscard]] std::int64_t
days_from_civil(std::int64_t year, std::uint32_t month, std::uint32_t day);

/// The civil date @p days after 1899-12-30, as year, month and day.
[[nodiscard]] std::tuple<std::int64_t, std::uint32_t, std::uint32_t>
civil_from_days(std::int64_t days);

/// A serial counted from @p epoch as days since 1899-12-30. The 1900 system
/// counts 1900-02-29, which never was, so a serial before 61 is one day more.
[[nodiscard]] double days_from_serial(double serial, Epoch epoch);
/// The serial @p epoch counts for @p days since 1899-12-30.
[[nodiscard]] double serial_from_days(double days, Epoch epoch);

/// The signs a locale writes a number with, where the code spells `.` and `,`,
/// and the names it writes a date with.
struct Symbols final {
  std::string decimal{"."};
  std::string group{","};
  const CalendarNames *names{nullptr}; ///< English where it is null
};

/// The symbols of the BCP 47 tag @p locale, from a small table of languages;
/// one it does not name writes `.` and `,` and English names.
[[nodiscard]] Symbols symbols_of(std::string_view locale);

/// What the first section of a format shows a number as.
enum class Category { number, date, time };

/// A parsed MS-XLS 2.4.126 format. Colors are retained but not rendered; `*`
/// adds no fill.
class Format final {
public:
  /// `General`.
  Format();
  /// @throws std::invalid_argument where @p code is no format code.
  explicit Format(std::string_view code);

  /// The text a cell holding @p value shows, with @p symbols for the point,
  /// the grouping comma and the names of a date. A date or time section reads
  /// it as a serial counted from @p epoch.
  [[nodiscard]] std::string format(double value, Epoch epoch = Epoch::from_1900,
                                   const Symbols &symbols = {}) const;
  /// The text a cell holding the text @p value shows.
  [[nodiscard]] std::string format(std::string_view value) const;

  [[nodiscard]] Category category() const;

private:
  std::vector<Section> m_sections;
};

/// `General`: up to 15 significant digits, in scientific notation outside
/// [1e-10, 1e15).
[[nodiscard]] std::string format_general(double value,
                                         const Symbols &symbols = {});

} // namespace odr::internal::number_format
