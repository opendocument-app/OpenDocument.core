#include <odr/internal/formula/formula_function.hpp>
#include <odr/internal/number_format/number_format.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

namespace odr::internal::formula {

namespace {

/// Argument @p index as a whole number, toward 0. No answer where reading it
/// to 15 digits first gives another one: LibreOffice does (`double_to`), and
/// Excel does not document it.
Result<double> whole_argument(const Call &call, const std::size_t index) {
  const Result<double> x = number_argument(call, index);
  if (!x.has_value()) {
    return x;
  }
  const double whole = std::trunc(*x);
  if (whole != std::trunc(snapped(*x, 15))) {
    return no_answer();
  }
  return whole;
}

/// The most a 16-bit argument of LibreOffice holds; past it is `Err:502`.
constexpr double int16_limit = 32767;

/// The first serial Excel counts a real day for: before it lies the
/// 1900-02-29 that never was.
constexpr double first_true_serial = 61;

/// The first serial past the dates of @p call: 10000-01-01 in Excel, and in
/// LibreOffice the first day past its 16-bit years.
double serial_limit(const Call &call) noexcept {
  const std::int64_t year = is_libreoffice(call) ? 32768 : 10000;
  return call.settings().serial(
      static_cast<double>(number_format::days_from_civil(year, 1, 1)));
}

/// The year and the month counted from 1 of @p months since the year 0.
std::pair<std::int64_t, std::uint32_t>
month_of(const std::int64_t months) noexcept {
  const std::int64_t year = months >= 0 ? months / 12 : -((-months + 11) / 12);
  return {year, static_cast<std::uint32_t>(months - year * 12 + 1)};
}

/// Converts @p serial to days since 1899-12-30. Refuses times near midnight
/// whose rounding may change the date.
Result<std::int64_t> day_of(const Call &call, const double serial) noexcept {
  const bool excel_1900 =
      !is_libreoffice(call) &&
      call.settings().epoch == number_format::Epoch::from_1900;
  if (!is_libreoffice(call) && (serial < 0 || serial >= serial_limit(call))) {
    return error_result(ErrorType::number);
  }
  if (std::abs(serial) >= serial_limit(call) ||
      (excel_1900 && serial < first_true_serial)) {
    return no_answer();
  }
  const double day = std::floor(serial);
  const double rounded = std::floor(std::round(serial * 86400) / 86400);
  if (day != rounded) {
    return no_answer();
  }
  return static_cast<std::int64_t>(call.settings().days(day));
}

/// The days since 1899-12-30 that argument @p index stands for, as `day_of`
/// reads it.
Result<std::int64_t> day_argument(const Call &call, const std::size_t index) {
  return number_argument(call, index).and_then([&](const double serial) {
    return day_of(call, serial);
  });
}

/// The serial of the day @p days after 1899-12-30, as a formula computes it.
Result<Value> serial_of(const Call &call, const std::int64_t days) noexcept {
  const double serial = call.settings().serial(static_cast<double>(days));
  if (!is_libreoffice(call)) {
    if (serial < 0 || serial >= serial_limit(call)) {
      return Value{ErrorType::number};
    }
    if (call.settings().epoch == number_format::Epoch::from_1900 &&
        serial < first_true_serial) {
      return no_answer();
    }
  }
  return Value{serial};
}

/// The days of month @p month of @p year.
std::uint32_t month_length(const std::int64_t year,
                           const std::uint32_t month) noexcept {
  const std::int64_t next =
      month == 12 ? number_format::days_from_civil(year + 1, 1, 1)
                  : number_format::days_from_civil(year, month + 1, 1);
  return static_cast<std::uint32_t>(
      next - number_format::days_from_civil(year, month, 1));
}

/// The year, the month counted from 1, and the day @p months after the
/// month of @p days.
Result<std::tuple<std::int64_t, std::uint32_t, std::uint32_t>>
shifted(const std::int64_t days, const double months) noexcept {
  if (std::abs(months) > 1e6) {
    return no_answer();
  }
  const auto [year, month, day] = number_format::civil_from_days(days);
  const auto [new_year, new_month] =
      month_of(year * 12 + (month - 1) + static_cast<std::int64_t>(months));
  return std::tuple{new_year, new_month, day};
}

/// `DATE`: a year, a month and a day, which may run over into the next.
/// LibreOffice reads each as a 16-bit integer (`GetInt16`).
Result<Value> date(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 3, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<double> stated_year = whole_argument(call, 0);
  if (!stated_year.has_value()) {
    return std::unexpected(stated_year.error());
  }
  const Result<double> stated_month = whole_argument(call, 1);
  if (!stated_month.has_value()) {
    return std::unexpected(stated_month.error());
  }
  const Result<double> stated_day = whole_argument(call, 2);
  if (!stated_day.has_value()) {
    return std::unexpected(stated_day.error());
  }
  double year = *stated_year;
  const double month = *stated_month;
  const double day = *stated_day;
  if (is_libreoffice(call)) {
    if (year < 0 || year > int16_limit || std::abs(month) > int16_limit ||
        std::abs(day) > int16_limit) {
      return no_answer();
    }
    const std::int64_t null_year = call.settings().null_year;
    if (year < 100) {
      // the century that puts the year at or past the null year
      const std::int64_t century = null_year - null_year % 100;
      year += static_cast<double>(year >= static_cast<double>(null_year % 100)
                                      ? century
                                      : century + 100);
    } else if (year < 1583) {
      return no_answer(); // `#VALUE!` here, by a rule not found yet
    }
  } else {
    if (year < 0 || year >= 10000) {
      return Value{ErrorType::number};
    }
    if (year < 1900) {
      year += 1900;
    }
  }
  if (std::abs(month) > 1e6 || std::abs(day) > 1e8) {
    return no_answer();
  }
  const auto [first_year, first_month] =
      month_of(static_cast<std::int64_t>(year) * 12 +
               static_cast<std::int64_t>(month) - 1);
  const std::int64_t days =
      number_format::days_from_civil(first_year, first_month, 1) +
      static_cast<std::int64_t>(day) - 1;
  if (is_libreoffice(call) &&
      std::get<0>(number_format::civil_from_days(days)) > int16_limit) {
    return no_answer();
  }
  return serial_of(call, days);
}

/// `TIME`: hours, minutes and seconds as a fraction of a day, past a whole
/// day from 0 again. Excel truncates each argument, LibreOffice does not.
Result<Value> time(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 3, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const bool whole = !is_libreoffice(call);
  const auto read = [&](const std::size_t index) {
    return whole ? whole_argument(call, index) : number_argument(call, index);
  };
  const Result<double> hours = read(0);
  if (!hours.has_value()) {
    return std::unexpected(hours.error());
  }
  const Result<double> minutes = read(1);
  if (!minutes.has_value()) {
    return std::unexpected(minutes.error());
  }
  const Result<double> seconds = read(2);
  if (!seconds.has_value()) {
    return std::unexpected(seconds.error());
  }
  // Excel takes each part up to 32767
  if (whole && std::max({*hours, *minutes, *seconds}) > 32767) {
    return Value{ErrorType::number};
  }
  const double total = *hours * 3600 + *minutes * 60 + *seconds;
  if (total < 0) {
    return refused(call, ErrorType::number);
  }
  return Value{std::fmod(total, 86400) / 86400};
}

enum class Part { year, month, day };

template <Part part> Result<Value> date_part(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::int64_t> days = day_argument(call, 0);
  if (!days.has_value()) {
    return std::unexpected(days.error());
  }
  const auto [year, month, day] = number_format::civil_from_days(*days);
  switch (part) {
  case Part::year:
    return Value{static_cast<double>(year)};
  case Part::month:
    return Value{static_cast<double>(month)};
  case Part::day:
    return Value{static_cast<double>(day)};
  }
  return Value{0.0};
}

/// The second of the day @p serial falls on, which both applications round
/// in a way not documented: no answer where rounding and truncating to the
/// second give another @p part.
template <std::int64_t unit, std::int64_t count>
Result<Value> time_part(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<double> serial = number_argument(call, 0);
  if (!serial.has_value()) {
    return std::unexpected(serial.error());
  }
  if (!is_libreoffice(call) && *serial < 0) {
    return Value{ErrorType::number};
  }
  const double fraction = *serial - std::floor(*serial);
  const auto part = [&](const double seconds) {
    const auto whole = static_cast<std::int64_t>(seconds) % 86400;
    return whole / unit % count;
  };
  const std::int64_t rounded = part(std::round(fraction * 86400));
  if (rounded != part(std::floor(fraction * 86400))) {
    return no_answer();
  }
  return Value{static_cast<double>(rounded)};
}

/// `WEEKDAY`: the day of the week by the numbering @p type names.
Result<Value> weekday(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::int64_t> days = day_argument(call, 0);
  if (!days.has_value()) {
    return std::unexpected(days.error());
  }
  double type = 1;
  if (call.size() > 1) {
    const Result<double> stated = whole_argument(call, 1);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    type = *stated;
  }
  // 1899-12-30 was a Saturday; 0 is Sunday here
  const std::int64_t sunday_based = ((*days % 7) + 7 + 6) % 7;
  const std::int64_t monday_based = (sunday_based + 6) % 7;
  if (type == 1 || type == 17) {
    return Value{static_cast<double>(sunday_based + 1)};
  }
  if (type == 2 || type == 11) {
    return Value{static_cast<double>(monday_based + 1)};
  }
  if (type == 3) {
    return Value{static_cast<double>(monday_based)};
  }
  if (type >= 12 && type <= 16) {
    // the week starts on Tuesday for 12, and so on to Saturday for 16
    const auto first = static_cast<std::int64_t>(type) - 11;
    return Value{static_cast<double>((monday_based - first + 7) % 7 + 1)};
  }
  return refused(call, ErrorType::number);
}

/// `EDATE` and `EOMONTH`: a date some months on, on its day or on the last
/// day of the month.
template <bool end_of_month> Result<Value> months_on(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::int64_t> days = day_argument(call, 0);
  if (!days.has_value()) {
    return std::unexpected(days.error());
  }
  const Result<double> months = whole_argument(call, 1);
  if (!months.has_value()) {
    return std::unexpected(months.error());
  }
  const auto moved = shifted(*days, *months);
  if (!moved.has_value()) {
    return std::unexpected(moved.error());
  }
  const auto [year, month, day] = *moved;
  const std::uint32_t length = month_length(year, month);
  return serial_of(
      call, number_format::days_from_civil(
                year, month, end_of_month ? length : std::min(day, length)));
}

/// `DAYS`: the days from the second date to the first. Excel counts whole
/// days, and refuses a date out of its range, LibreOffice the difference of
/// the serials.
Result<Value> days(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<double> end = number_argument(call, 0);
  if (!end.has_value()) {
    return std::unexpected(end.error());
  }
  const Result<double> start = number_argument(call, 1);
  if (!start.has_value()) {
    return std::unexpected(start.error());
  }
  if (is_libreoffice(call)) {
    return Value{*end - *start};
  }
  for (const double serial : {*end, *start}) {
    if (serial < 0 || serial >= serial_limit(call)) {
      return Value{ErrorType::number};
    }
  }
  return Value{std::trunc(*end) - std::trunc(*start)};
}

/// `DATEDIF` in whole years, months or days. The units that count from a
/// date in another month or year (`MD`, `YM`, `YD`) have no answer, as
/// Excel documents one of them as wrong.
Result<Value> date_difference(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 3, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::int64_t> start = day_argument(call, 0);
  if (!start.has_value()) {
    return std::unexpected(start.error());
  }
  const Result<std::int64_t> end = day_argument(call, 1);
  if (!end.has_value()) {
    return std::unexpected(end.error());
  }
  const Result<Text> unit = call.scalar(2).and_then(
      [&](const Value &value) { return call.text(value); });
  if (!unit.has_value()) {
    return std::unexpected(unit.error());
  }
  if (const auto *error = std::get_if<ErrorType>(&*unit)) {
    return Value{*error};
  }
  if (*end < *start) {
    return refused(call, ErrorType::number);
  }
  const std::string &spelled = *std::get_if<std::string>(&*unit);
  if (spelled == "D" || spelled == "d") {
    return Value{static_cast<double>(*end - *start)};
  }
  const auto [start_year, start_month, start_day] =
      number_format::civil_from_days(*start);
  const auto [end_year, end_month, end_day] =
      number_format::civil_from_days(*end);
  std::int64_t months = (end_year - start_year) * 12 +
                        (static_cast<std::int64_t>(end_month) - start_month);
  if (end_day < start_day) {
    --months;
  }
  if (spelled == "M" || spelled == "m") {
    return Value{static_cast<double>(months)};
  }
  if (spelled == "Y" || spelled == "y") {
    const std::int64_t years = months / 12;
    return Value{static_cast<double>(years)};
  }
  return no_answer();
}

/// `NOW` and `TODAY`: the moment the caller states, without the time for
/// `TODAY`.
template <bool with_time> Result<Value> now(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 0, 0);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const std::optional<double> days = call.settings().now;
  if (!days.has_value()) {
    return no_answer();
  }
  return Value{call.settings().serial(with_time ? *days : std::floor(*days))};
}

constexpr std::array entries{
    FunctionEntry{"DATE", date},
    FunctionEntry{"DATEDIF", date_difference},
    FunctionEntry{"DAY", date_part<Part::day>},
    FunctionEntry{"DAYS", days},
    FunctionEntry{"EDATE", months_on<false>},
    FunctionEntry{"EOMONTH", months_on<true>},
    FunctionEntry{"HOUR", time_part<3600, 24>},
    FunctionEntry{"MINUTE", time_part<60, 60>},
    FunctionEntry{"MONTH", date_part<Part::month>},
    FunctionEntry{"NOW", now<true>},
    FunctionEntry{"SECOND", time_part<1, 60>},
    FunctionEntry{"TIME", time},
    FunctionEntry{"TODAY", now<false>},
    FunctionEntry{"WEEKDAY", weekday},
    FunctionEntry{"YEAR", date_part<Part::year>},
};

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

std::span<const formula::FunctionEntry> formula::date_functions() {
  return entries;
}

} // namespace odr::internal
