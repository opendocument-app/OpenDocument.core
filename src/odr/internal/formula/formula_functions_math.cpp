#include <odr/internal/formula/formula_function.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace odr::internal::formula {

namespace {

/// An argument outside the domain of a function.
Result<Value> invalid(const Call &call) noexcept {
  return refused(call, ErrorType::number);
}

Value checked(const double x) noexcept {
  return std::isfinite(x) ? Value{x} : Value{ErrorType::number};
}

/// @p round applied to @p x read to 15 significant digits. No answer where
/// reading x to 12 digits rounds it elsewhere: x is so close to the edge
/// that the two applications decide apart.
template <typename Round>
Result<double> stable(const double x, const Round &round) noexcept {
  const double result = round(snapped(x, 15));
  if (round(snapped(x, 12)) != result) {
    return no_answer();
  }
  return result;
}

/// @p round applied as `stable` does. Excel documents no such reading for a
/// function other than a rounding, so there a number it moves has no answer.
template <typename Round>
Result<double> settled(const Call &call, const double x,
                       const Round &round) noexcept {
  const Result<double> result = stable(x, round);
  if (!result.has_value()) {
    return result;
  }
  if (!is_libreoffice(call) && round(x) != *result) {
    return no_answer();
  }
  return result;
}

double round_half_away(const double x) noexcept { return std::round(x); }
double round_away(const double x) noexcept {
  return x < 0 ? -std::ceil(-x) : std::ceil(x);
}
double round_toward(const double x) noexcept { return std::trunc(x); }

/// @p x rounded to @p digits after the decimal point, by @p round.
template <typename Round>
Result<Value> round_to(const double x, const double digits,
                       const Round &round) noexcept {
  const double places = std::trunc(digits);
  if (std::abs(places) > 300) {
    return no_answer();
  }
  if (places >= 0) {
    const double scale = std::pow(10.0, places);
    if (!std::isfinite(x * scale)) {
      return Value{x};
    }
    return stable(x * scale, round).transform([&](const double rounded) {
      return checked(rounded / scale);
    });
  }
  const double scale = std::pow(10.0, -places);
  return stable(x / scale, round).transform([&](const double rounded) {
    return checked(rounded * scale);
  });
}

using Unary = Result<Value> (*)(const Call &, double);
using Binary = Result<Value> (*)(const Call &, double, double);

/// A function of one number.
template <Unary function> Result<Value> unary(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  return number_argument(call, 0).and_then(
      [&](const double x) { return function(call, x); });
}

/// A function of two numbers, the second @p fallback where it is left out.
template <Binary function, std::int32_t fallback = 0, bool optional = false>
Result<Value> binary(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, optional ? 1 : 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<double> x = number_argument(call, 0);
  if (!x.has_value()) {
    return std::unexpected(x.error());
  }
  double y = fallback;
  if (call.size() > 1) {
    const Result<double> second = number_argument(call, 1);
    if (!second.has_value()) {
      return std::unexpected(second.error());
    }
    y = *second;
  }
  return function(call, *x, y);
}

/// The numbers an aggregate reads out of its arguments, and the first error
/// among them.
struct Collected final {
  std::vector<double> numbers{};
  std::optional<ErrorType> error{};
};

/// Reads @p count arguments as SUM does: numeric cells and LibreOffice
/// boolean cells; direct booleans and Excel numeric text also count.
Result<Collected> collect(const Call &call, const std::size_t count) {
  Collected result;
  for (std::size_t i = 0; i < count; ++i) {
    if (call.missing(i)) {
      return no_answer();
    }
    Errors errors;
    if (const Result<void> walked = call.for_each_value(
            i,
            [&](const Value &value, const bool stated) -> Result<void> {
              if (const auto *number = std::get_if<double>(&value.content)) {
                result.numbers.push_back(*number);
              } else if (const auto *boolean =
                             std::get_if<bool>(&value.content)) {
                if (stated || is_libreoffice(call)) {
                  result.numbers.push_back(*boolean ? 1 : 0);
                }
              } else if (const auto *error =
                             std::get_if<ErrorType>(&value.content)) {
                errors.add(*error);
              } else if (!stated) {
                return {};
              } else if (const auto *text =
                             std::get_if<std::string>(&value.content)) {
                // LibreOffice refuses a text argument with `#VALUE!` or a code
                // of its own, function by function
                if (is_libreoffice(call)) {
                  return no_answer();
                }
                const std::optional<Number> read = number_of_text(*text);
                if (!read.has_value()) {
                  return no_answer();
                }
                if (const auto *error = std::get_if<ErrorType>(&*read)) {
                  errors.add(*error);
                } else {
                  result.numbers.push_back(*std::get_if<double>(&*read));
                }
              } else {
                return no_answer();
              }
              return {};
            });
        !walked.has_value()) {
      return std::unexpected(walked.error());
    }
    if (!result.error.has_value()) {
      const Result<std::optional<ErrorType>> first = errors.first();
      if (!first.has_value()) {
        return std::unexpected(first.error());
      }
      result.error = *first;
    }
  }
  return result;
}

Result<Collected> collect(const Call &call) {
  return collect(call, call.size());
}

/// A sum as LibreOffice's `KahanSum` adds it (`sc/inc/kahan.hxx`): a
/// Neumaier sum that holds the last number back, and gives 0 where that
/// number cancels the rest as `approxAdd` decides.
class KahanSum final {
public:
  void add(const double x) {
    if (x == 0) {
      return;
    }
    if (m_last != 0) {
      neumaier(m_last);
    }
    m_last = x;
  }

  [[nodiscard]] double get() const {
    const double total = m_sum + m_error;
    if (m_last == 0) {
      return total;
    }
    if (total != 0 && (m_last < 0) != (total < 0) &&
        approximately_equal(m_last, -total)) {
      return 0;
    }
    KahanSum result = *this;
    result.neumaier(m_last);
    return result.m_sum + result.m_error;
  }

private:
  double m_sum{0};
  double m_error{0};
  double m_last{0};

  void neumaier(const double x) {
    const double t = m_sum + x;
    m_error +=
        std::abs(m_sum) >= std::abs(x) ? (m_sum - t) + x : (x - t) + m_sum;
    m_sum = t;
  }
};

double kahan_sum(const std::span<const double> numbers) noexcept {
  KahanSum sum;
  for (const double number : numbers) {
    sum.add(number);
  }
  return sum.get();
}

Result<Value> sum(const Call &call) {
  const Result<Collected> collected = collect(call);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (collected->error.has_value()) {
    return Value{*collected->error};
  }
  return sum_of(call, collected->numbers);
}

Result<Value> average(const Call &call) {
  const Result<Collected> collected = collect(call);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (collected->error.has_value()) {
    return Value{*collected->error};
  }
  if (collected->numbers.empty()) {
    return Value{ErrorType::division};
  }
  Result<Value> total = sum_of(call, collected->numbers);
  if (!total.has_value() || !total->holds<double>()) {
    return total;
  }
  return checked(total->get<double>() /
                 static_cast<double>(collected->numbers.size()));
}

template <bool largest> Result<Value> extreme(const Call &call) {
  const Result<Collected> collected = collect(call);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (collected->error.has_value()) {
    return Value{*collected->error};
  }
  if (collected->numbers.empty()) {
    return Value{0.0};
  }
  return Value{largest ? std::ranges::max(collected->numbers)
                       : std::ranges::min(collected->numbers)};
}

Result<Value> product(const Call &call) {
  const Result<Collected> collected = collect(call);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (collected->error.has_value()) {
    return Value{*collected->error};
  }
  if (collected->numbers.empty()) {
    return Value{0.0};
  }
  double result = 1;
  for (const double number : collected->numbers) {
    result *= number;
  }
  return checked(result);
}

Result<Value> sum_of_squares(const Call &call) {
  const Result<Collected> collected = collect(call);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (collected->error.has_value()) {
    return Value{*collected->error};
  }
  std::vector<double> squares;
  squares.reserve(collected->numbers.size());
  for (const double number : collected->numbers) {
    squares.push_back(number * number);
  }
  return sum_of(call, squares);
}

/// The numbers of an aggregate that needs at least one, where none is
/// `#VALUE!` in LibreOffice and `#NUM!` in Excel.
std::optional<Value> no_numbers(const Call &call,
                                const Collected &collected) noexcept {
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  if (collected.numbers.empty()) {
    return Value{is_libreoffice(call) ? ErrorType::value : ErrorType::number};
  }
  return std::nullopt;
}

Result<Value> median(const Call &call) {
  Result<Collected> collected = collect(call);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (const std::optional<Value> refused = no_numbers(call, *collected);
      refused.has_value()) {
    return *refused;
  }
  std::vector<double> &numbers = collected->numbers;
  std::ranges::sort(numbers);
  const std::size_t middle = numbers.size() / 2;
  if (numbers.size() % 2 == 1) {
    return Value{numbers[middle]};
  }
  return checked((numbers[middle - 1] + numbers[middle]) / 2);
}

/// The @p k th largest or smallest of the first argument. LibreOffice reads a
/// fraction of k up, Excel's rule for one is not documented.
template <bool largest> Result<Value> kth(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  Result<Collected> collected = collect(call, 1);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (const std::optional<Value> refused = no_numbers(call, *collected);
      refused.has_value()) {
    return *refused;
  }
  const Result<double> read = number_argument(call, 1);
  if (!read.has_value()) {
    return std::unexpected(read.error());
  }
  double rank = *read;
  if (rank != std::trunc(rank)) {
    if (!is_libreoffice(call)) {
      return no_answer();
    }
    rank = std::ceil(rank);
  }
  std::vector<double> &numbers = collected->numbers;
  if (rank < 1 || rank > static_cast<double>(numbers.size())) {
    return invalid(call);
  }
  std::ranges::sort(numbers);
  const auto at = static_cast<std::size_t>(rank) - 1;
  return Value{largest ? numbers[numbers.size() - 1 - at] : numbers[at]};
}

/// The variance of the numbers, of a sample where @p sample. LibreOffice
/// takes the mean with `KahanSum` and each deviation with `approxSub`.
template <bool sample, bool root> Result<Value> variance_of(const Call &call) {
  const Result<Collected> collected = collect(call);
  if (!collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (collected->error.has_value()) {
    return Value{*collected->error};
  }
  const std::vector<double> &numbers = collected->numbers;
  const std::size_t least = sample ? 2 : 1;
  if (numbers.size() < least) {
    return Value{ErrorType::division};
  }
  const bool libreoffice = is_libreoffice(call);
  double mean = 0;
  if (libreoffice) {
    mean = kahan_sum(numbers);
  } else {
    for (const double number : numbers) {
      mean += number;
    }
  }
  mean /= static_cast<double>(numbers.size());
  double squares = 0;
  for (const double number : numbers) {
    const double deviation =
        libreoffice ? approximate_add(number, -mean) : number - mean;
    squares += deviation * deviation;
  }
  const double variance =
      squares / static_cast<double>(numbers.size() - (sample ? 1 : 0));
  return checked(root ? std::sqrt(variance) : variance);
}

Result<Value> count(const Call &call) {
  double result = 0;
  for (std::size_t i = 0; i < call.size(); ++i) {
    if (call.missing(i)) {
      return no_answer();
    }
    if (const Result<void> walked = call.for_each_value(
            i,
            [&](const Value &value, const bool stated) -> Result<void> {
              if (const auto *text = std::get_if<std::string>(&value.content);
                  text != nullptr && stated) {
                const std::optional<Number> read = number_of_text(*text);
                if (!read.has_value()) {
                  return no_answer();
                }
                result += std::holds_alternative<double>(*read) ? 1 : 0;
                return {};
              }
              result +=
                  value.holds<double>() || (value.holds<bool>() &&
                                            (stated || is_libreoffice(call)))
                      ? 1
                      : 0;
              return {};
            });
        !walked.has_value()) {
      return std::unexpected(walked.error());
    }
  }
  return Value{result};
}

Result<Value> count_stated(const Call &call) {
  double result = 0;
  for (std::size_t i = 0; i < call.size(); ++i) {
    if (call.missing(i)) {
      return no_answer();
    }
    if (const Result<void> walked = call.for_each_value(
            i,
            [&](const Value &value, const bool stated) -> Result<void> {
              result += stated || !value.holds<Empty>() ? 1 : 0;
              return {};
            });
        !walked.has_value()) {
      return std::unexpected(walked.error());
    }
  }
  return Value{result};
}

/// Empty cells, and in Excel the cells holding an empty text. Whether
/// LibreOffice counts an empty text depends on whether a formula made it.
Result<Value> count_blank(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> value = call.value(0);
  if (!value.has_value()) {
    return std::unexpected(value.error());
  }
  const auto *reference = std::get_if<Reference>(&value->content);
  // a whole column counts the blank cells of the grid, which the extent of
  // its sheet cuts
  if (reference == nullptr || reference->areas.size() != 1 ||
      reference->areas.front().whole_columns ||
      reference->areas.front().whole_rows) {
    return no_answer();
  }
  // the cells past the extent of the sheet are empty, and not visited
  const TableRange &range = reference->areas.front().range;
  const double cells =
      (static_cast<double>(range.to().column) - range.from().column + 1) *
      (static_cast<double>(range.to().row) - range.from().row + 1);
  double stated = 0;
  if (const Result<void> walked = call.for_each(
          *reference,
          [&](const SheetPosition &, const Value &cell) -> Result<void> {
            const auto *text = std::get_if<std::string>(&cell.content);
            if (text != nullptr && text->empty()) {
              if (is_libreoffice(call)) {
                return no_answer();
              }
              return {};
            }
            stated += cell.holds<Empty>() ? 0 : 1;
            return {};
          });
      !walked.has_value()) {
    return std::unexpected(walked.error());
  }
  return Value{cells - stated};
}

/// `SUMPRODUCT`: the arrays element by element. A text and an empty cell are
/// 0, and so is a boolean in Excel.
Result<Value> sum_product(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 1, 255);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  std::vector<Matrix> arrays;
  for (std::size_t i = 0; i < call.size(); ++i) {
    Result<Matrix> array = call.array(i);
    if (!array.has_value()) {
      return std::unexpected(array.error());
    }
    arrays.push_back(std::move(*array));
    if (arrays.back().columns != arrays.front().columns ||
        arrays.back().rows != arrays.front().rows) {
      return Value{ErrorType::value};
    }
  }
  Errors errors;
  std::vector<double> products(arrays.front().cells.size(), 1);
  for (const Matrix &array : arrays) {
    for (std::size_t i = 0; i < array.cells.size(); ++i) {
      const Value &cell = array.cells[i];
      double factor = 0;
      if (const auto *number = std::get_if<double>(&cell.content)) {
        factor = *number;
      } else if (const auto *boolean = std::get_if<bool>(&cell.content)) {
        factor = *boolean && is_libreoffice(call) ? 1 : 0;
      } else if (const auto *error = std::get_if<ErrorType>(&cell.content)) {
        errors.add(*error);
      }
      products[i] *= factor;
    }
  }
  const Result<std::optional<ErrorType>> error = errors.first();
  if (!error.has_value()) {
    return std::unexpected(error.error());
  }
  if (error->has_value()) {
    return Value{**error};
  }
  return sum_of(call, products);
}

Result<Value> absolute(const Call &, const double x) noexcept {
  return Value{std::abs(x)};
}

Result<Value> sign(const Call &, const double x) noexcept {
  return Value{x > 0 ? 1.0 : x < 0 ? -1.0 : 0.0};
}

Result<Value> integer(const Call &call, const double x) noexcept {
  return settled(call, x, [](const double y) { return std::floor(y); })
      .transform(checked);
}

Result<Value> round_function(const Call &, const double x,
                             const double digits) noexcept {
  return round_to(x, digits, round_half_away);
}
Result<Value> round_up(const Call &, const double x,
                       const double digits) noexcept {
  return round_to(x, digits, round_away);
}
Result<Value> round_down(const Call &, const double x,
                         const double digits) noexcept {
  return round_to(x, digits, round_toward);
}

Result<Value> modulo(const Call &call, const double x,
                     const double y) noexcept {
  if (y == 0) {
    return Value{ErrorType::division};
  }
  const double quotient = x / y;
  // Excel refuses a quotient this large in some versions
  if (!is_libreoffice(call) && std::abs(quotient) >= 134217728.0) {
    return no_answer();
  }
  const Result<double> floor =
      settled(call, quotient, [](const double q) { return std::floor(q); });
  if (!floor.has_value()) {
    return std::unexpected(floor.error());
  }
  if (!is_libreoffice(call)) {
    const double result = x - y * *floor;
    if (nearly_cancels(result, std::abs(x))) {
      return no_answer();
    }
    return checked(result);
  }
  // LibreOffice refuses a remainder that precision put outside the divisor
  const double result = approximate_add(x, -y * *floor);
  if (y > 0 ? result < 0 || result >= y : result > 0 || result <= y) {
    return Value{ErrorType::value};
  }
  return checked(result);
}

Result<Value> quotient(const Call &call, const double x,
                       const double y) noexcept {
  if (y == 0) {
    if (is_libreoffice(call)) {
      return no_answer();
    }
    return Value{ErrorType::division};
  }
  return settled(call, x / y, round_toward).transform(checked);
}

Result<Value> square_root(const Call &call, const double x) noexcept {
  if (x < 0) {
    return invalid(call);
  }
  return Value{std::sqrt(x)};
}

Result<Value> exponential(const Call &, const double x) noexcept {
  return checked(std::exp(x));
}

Result<Value> natural_logarithm(const Call &call, const double x) noexcept {
  if (x <= 0) {
    return invalid(call);
  }
  return Value{std::log(x)};
}

Result<Value> logarithm(const Call &call, const double x,
                        const double base) noexcept {
  if (x <= 0 || base <= 0) {
    return invalid(call);
  }
  if (base == 1) {
    if (is_libreoffice(call)) {
      return no_answer();
    }
    return Value{ErrorType::division};
  }
  return checked(std::log(x) / std::log(base));
}

Result<Value> logarithm_10(const Call &call, const double x) noexcept {
  if (x <= 0) {
    return invalid(call);
  }
  return Value{std::log10(x)};
}

Result<Value> power_function(const Call &call, const double x,
                             const double y) noexcept {
  return power(x, y, call.settings().dialect);
}

Result<Value> pi(const Call &call) noexcept {
  return expect_arguments(call, 0, 0).transform([] {
    return Value{std::numbers::pi};
  });
}

Result<Value> radians(const Call &, const double x) noexcept {
  return checked(x * std::numbers::pi / 180);
}
Result<Value> degrees(const Call &, const double x) noexcept {
  return checked(x * 180 / std::numbers::pi);
}
/// @p result, a sine, cosine or tangent of @p x. Excel refuses an angle of
/// 2^27 or more, at a limit it does not document, so it has no answer.
Result<Value> circular(const double x, const double result) noexcept {
  if (std::abs(x) >= 134217728.0) {
    return no_answer();
  }
  return checked(result);
}
Result<Value> sine(const Call &, const double x) noexcept {
  return circular(x, std::sin(x));
}
Result<Value> cosine(const Call &, const double x) noexcept {
  return circular(x, std::cos(x));
}
Result<Value> tangent(const Call &, const double x) noexcept {
  return circular(x, std::tan(x));
}
Result<Value> hyperbolic_sine(const Call &, const double x) noexcept {
  return checked(std::sinh(x));
}
Result<Value> hyperbolic_cosine(const Call &, const double x) noexcept {
  return checked(std::cosh(x));
}
Result<Value> hyperbolic_tangent(const Call &, const double x) noexcept {
  return checked(std::tanh(x));
}
Result<Value> arc_tangent(const Call &, const double x) noexcept {
  return Value{std::atan(x)};
}

Result<Value> arc_sine(const Call &, const double x) noexcept {
  if (x < -1 || x > 1) {
    return Value{ErrorType::number};
  }
  return Value{std::asin(x)};
}

Result<Value> arc_cosine(const Call &, const double x) noexcept {
  if (x < -1 || x > 1) {
    return Value{ErrorType::number};
  }
  return Value{std::acos(x)};
}

/// `ATAN2(x; y)`: the angle of the point (x, y), which both applications
/// take in this order.
Result<Value> arc_tangent_2(const Call &call, const double x,
                            const double y) noexcept {
  if (x == 0 && y == 0) {
    return is_libreoffice(call) ? Value{0.0} : Value{ErrorType::division};
  }
  return Value{std::atan2(y, x)};
}

/// `EVEN` and `ODD`: away from 0 to the next even or odd integer.
template <bool odd>
Result<Value> parity(const Call &call, const double x) noexcept {
  const Result<double> away = settled(call, x, round_away);
  if (!away.has_value()) {
    return std::unexpected(away.error());
  }
  const double magnitude = std::abs(*away);
  double result = magnitude;
  if (odd) {
    result = std::fmod(magnitude, 2) == 1 ? magnitude : magnitude + 1;
  } else {
    result = std::fmod(magnitude, 2) == 0 ? magnitude : magnitude + 1;
  }
  return checked(x < 0 ? -result : result);
}

Result<Value> factorial(const Call &call, const double x) noexcept {
  // LibreOffice reads -0.5 as -1, and Excel refuses any negative number
  if (x < 0) {
    return invalid(call);
  }
  const Result<double> n = settled(call, x, round_toward);
  if (!n.has_value()) {
    return std::unexpected(n.error());
  }
  if (*n > 170) {
    return Value{is_libreoffice(call) ? ErrorType::value : ErrorType::number};
  }
  double result = 1;
  for (std::uint32_t i = 2; i <= *n; ++i) {
    result *= i;
  }
  return Value{result};
}

/// A number in [0, 1) from the caller's source.
Result<double> random_of(const Call &call) {
  if (!call.settings().random) {
    return no_answer();
  }
  return call.settings().random();
}

Result<Value> random(const Call &call) {
  return expect_arguments(call, 0, 0)
      .and_then([&] { return random_of(call); })
      .transform([](const double x) { return Value{x}; });
}

/// `RANDBETWEEN`: a whole number between two. A bound that is no whole
/// number, or a bottom above the top, has no answer: LibreOffice refuses the
/// second, and neither application documents what they do.
Result<Value> random_between(const Call &call) {
  if (const Result<void> expected = expect_arguments(call, 2, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<double> bottom = number_argument(call, 0);
  if (!bottom.has_value()) {
    return std::unexpected(bottom.error());
  }
  const Result<double> top = number_argument(call, 1);
  if (!top.has_value()) {
    return std::unexpected(top.error());
  }
  if (*bottom != std::trunc(*bottom) || *top != std::trunc(*top) ||
      *bottom > *top) {
    return no_answer();
  }
  return random_of(call).transform([&](const double x) {
    return Value{*bottom + std::floor(x * (*top - *bottom + 1))};
  });
}

constexpr std::array entries{
    FunctionEntry{"ABS", unary<absolute>},
    FunctionEntry{"ACOS", unary<arc_cosine>},
    FunctionEntry{"ASIN", unary<arc_sine>},
    FunctionEntry{"ATAN", unary<arc_tangent>},
    FunctionEntry{"ATAN2", binary<arc_tangent_2>},
    FunctionEntry{"AVERAGE", average},
    FunctionEntry{"COS", unary<cosine>},
    FunctionEntry{"COSH", unary<hyperbolic_cosine>},
    FunctionEntry{"COUNT", count},
    FunctionEntry{"COUNTA", count_stated},
    FunctionEntry{"COUNTBLANK", count_blank},
    FunctionEntry{"DEGREES", unary<degrees>},
    FunctionEntry{"EVEN", unary<parity<false>>},
    FunctionEntry{"EXP", unary<exponential>},
    FunctionEntry{"FACT", unary<factorial>},
    FunctionEntry{"INT", unary<integer>},
    FunctionEntry{"LARGE", kth<true>},
    FunctionEntry{"LN", unary<natural_logarithm>},
    FunctionEntry{"LOG", binary<logarithm, 10, true>},
    FunctionEntry{"LOG10", unary<logarithm_10>},
    FunctionEntry{"MAX", extreme<true>},
    FunctionEntry{"MEDIAN", median},
    FunctionEntry{"MIN", extreme<false>},
    FunctionEntry{"MOD", binary<modulo>},
    FunctionEntry{"ODD", unary<parity<true>>},
    FunctionEntry{"PI", pi},
    FunctionEntry{"POWER", binary<power_function>},
    FunctionEntry{"PRODUCT", product},
    FunctionEntry{"QUOTIENT", binary<quotient>},
    FunctionEntry{"RADIANS", unary<radians>},
    FunctionEntry{"RAND", random},
    FunctionEntry{"RANDBETWEEN", random_between},
    FunctionEntry{"ROUND", binary<round_function, 0, true>},
    FunctionEntry{"ROUNDDOWN", binary<round_down, 0, true>},
    FunctionEntry{"ROUNDUP", binary<round_up, 0, true>},
    FunctionEntry{"SIGN", unary<sign>},
    FunctionEntry{"SIN", unary<sine>},
    FunctionEntry{"SINH", unary<hyperbolic_sine>},
    FunctionEntry{"SMALL", kth<false>},
    FunctionEntry{"SQRT", unary<square_root>},
    FunctionEntry{"STDEV", variance_of<true, true>},
    FunctionEntry{"STDEV.P", variance_of<false, true>},
    FunctionEntry{"STDEV.S", variance_of<true, true>},
    FunctionEntry{"STDEVP", variance_of<false, true>},
    FunctionEntry{"SUM", sum},
    FunctionEntry{"SUMPRODUCT", sum_product},
    FunctionEntry{"SUMSQ", sum_of_squares},
    FunctionEntry{"TAN", unary<tangent>},
    FunctionEntry{"TANH", unary<hyperbolic_tangent>},
    FunctionEntry{"TRUNC", binary<round_down, 0, true>},
    FunctionEntry{"VAR", variance_of<true, false>},
    FunctionEntry{"VAR.P", variance_of<false, false>},
    FunctionEntry{"VAR.S", variance_of<true, false>},
    FunctionEntry{"VARP", variance_of<false, false>},
};

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

formula::Result<formula::Value>
formula::sum_of(const Call &call,
                const std::span<const double> numbers) noexcept {
  if (is_libreoffice(call)) {
    return checked(kahan_sum(numbers));
  }
  double total = 0;
  double largest = 0;
  for (const double number : numbers) {
    total += number;
    largest = std::max(largest, std::abs(number));
  }
  if (nearly_cancels(total, largest)) {
    return no_answer();
  }
  return checked(total);
}

std::span<const formula::FunctionEntry> formula::math_functions() {
  return entries;
}

} // namespace odr::internal
