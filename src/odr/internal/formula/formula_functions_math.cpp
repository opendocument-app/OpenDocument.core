#include <odr/internal/formula/formula_function.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace odr::internal::formula {

namespace {

bool is_libreoffice(const Call &call) {
  return call.settings().dialect == Dialect::libreoffice;
}

/// An argument outside the domain of a function. Excel answers `#NUM!`, and
/// LibreOffice a code of its own (`Err:502`) that no file states as an error.
Value invalid(const Call &call) {
  if (is_libreoffice(call)) {
    throw NoAnswer{};
  }
  return Value{ErrorType::number};
}

Value checked(const double x) {
  return std::isfinite(x) ? Value{x} : Value{ErrorType::number};
}

void expect_arguments(const Call &call, const std::size_t least,
                      const std::size_t most) {
  if (call.size() < least || call.size() > most) {
    throw NoAnswer{};
  }
  for (std::size_t i = 0; i < call.size(); ++i) {
    if (call.missing(i)) {
      throw NoAnswer{};
    }
  }
}

/// @p x read to @p digits significant digits, as a sheet reads a number
/// before it rounds it.
double snapped(const double x, const int digits) {
  if (x == 0 || !std::isfinite(x)) {
    return x;
  }
  return std::strtod(fmt::format("{:.{}g}", x, digits).c_str(), nullptr);
}

/// @p round applied to @p x read to 15 significant digits. Nothing where
/// reading x to 12 digits rounds it elsewhere: x is so close to the edge
/// that the two applications decide apart.
template <typename Round> double stable(const double x, const Round &round) {
  const double result = round(snapped(x, 15));
  if (round(snapped(x, 12)) != result) {
    throw NoAnswer{};
  }
  return result;
}

double round_half_away(const double x) { return std::round(x); }
double round_away(const double x) {
  return x < 0 ? -std::ceil(-x) : std::ceil(x);
}
double round_toward(const double x) { return std::trunc(x); }

/// @p x rounded to @p digits after the decimal point, by @p round.
template <typename Round>
Value round_to(const double x, const double digits, const Round &round) {
  const double places = std::trunc(digits);
  if (std::abs(places) > 300) {
    throw NoAnswer{};
  }
  if (places >= 0) {
    const double scale = std::pow(10.0, places);
    if (!std::isfinite(x * scale)) {
      return Value{x};
    }
    return checked(stable(x * scale, round) / scale);
  }
  const double scale = std::pow(10.0, -places);
  return checked(stable(x / scale, round) * scale);
}

/// The argument at @p index as a number, or the error standing for it.
Number argument(const Call &call, const std::size_t index) {
  return call.number(call.scalar(index));
}

using Unary = Value (*)(const Call &, double);
using Binary = Value (*)(const Call &, double, double);

/// A function of one number.
template <Unary function> Value unary(const Call &call) {
  expect_arguments(call, 1, 1);
  const Number x = argument(call, 0);
  if (const auto *error = std::get_if<ErrorType>(&x)) {
    return Value{*error};
  }
  return function(call, std::get<double>(x));
}

/// A function of two numbers, the second @p fallback where it is left out.
template <Binary function, int fallback = 0, bool optional = false>
Value binary(const Call &call) {
  expect_arguments(call, optional ? 1 : 2, 2);
  const Number x = argument(call, 0);
  if (const auto *error = std::get_if<ErrorType>(&x)) {
    return Value{*error};
  }
  const Number y =
      call.size() > 1 ? argument(call, 1) : Number{double{fallback}};
  if (const auto *error = std::get_if<ErrorType>(&y)) {
    return Value{*error};
  }
  return function(call, std::get<double>(x), std::get<double>(y));
}

/// The numbers an aggregate reads out of its arguments, and the first error
/// among them.
struct Collected final {
  std::vector<double> numbers;
  std::optional<ErrorType> error;
};

/// The errors of one argument. A range holding two different errors has no
/// first one both applications agree on.
class Errors final {
public:
  void add(const ErrorType error) { m_seen.insert(error); }
  void into(std::optional<ErrorType> &first) const {
    if (first.has_value() || m_seen.empty()) {
      return;
    }
    if (m_seen.size() > 1) {
      throw NoAnswer{};
    }
    first = *m_seen.begin();
  }

private:
  std::set<ErrorType> m_seen;
};

/// What a cell of a range or an element of an array adds to an aggregate: a
/// number, and a boolean in LibreOffice, where it is the number 1 or 0. A
/// text and an empty cell add nothing.
void collect_cell(const Call &call, const Value &value, Collected &into,
                  Errors &errors) {
  if (const auto *number = std::get_if<double>(&value.content)) {
    into.numbers.push_back(*number);
  } else if (const auto *boolean = std::get_if<bool>(&value.content)) {
    if (is_libreoffice(call)) {
      into.numbers.push_back(*boolean ? 1 : 0);
    }
  } else if (const auto *error = std::get_if<ErrorType>(&value.content)) {
    errors.add(*error);
  }
}

/// The numbers of the arguments from @p first on, as `SUM` reads them. An
/// argument stated directly adds a boolean too, and in Excel a text that
/// reads as a number.
Collected collect(const Call &call, const std::size_t first = 0,
                  const std::size_t last = std::size_t(-1)) {
  Collected result;
  for (std::size_t i = first; i < std::min(call.size(), last); ++i) {
    if (call.missing(i)) {
      throw NoAnswer{};
    }
    Errors errors;
    const Value value = call.value(i);
    if (const auto *reference = std::get_if<Reference>(&value.content)) {
      call.for_each(*reference, [&](const SheetPosition &, const Value &cell) {
        collect_cell(call, cell, result, errors);
      });
    } else if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      for (const Value &cell : matrix->cells) {
        collect_cell(call, cell, result, errors);
      }
    } else if (const auto *number = std::get_if<double>(&value.content)) {
      result.numbers.push_back(*number);
    } else if (const auto *boolean = std::get_if<bool>(&value.content)) {
      result.numbers.push_back(*boolean ? 1 : 0);
    } else if (const auto *error = std::get_if<ErrorType>(&value.content)) {
      errors.add(*error);
    } else if (const auto *text = std::get_if<std::string>(&value.content)) {
      // LibreOffice refuses a text argument with `#VALUE!` or a code of its
      // own, function by function
      if (is_libreoffice(call)) {
        throw NoAnswer{};
      }
      const std::optional<Number> read = number_of_text(*text);
      if (!read.has_value()) {
        throw NoAnswer{};
      }
      if (const auto *error = std::get_if<ErrorType>(&*read)) {
        errors.add(*error);
      } else {
        result.numbers.push_back(std::get<double>(*read));
      }
    } else {
      throw NoAnswer{};
    }
    errors.into(result.error);
  }
  return result;
}

/// The sum of @p numbers. LibreOffice adds left to right and makes a sum
/// that cancels 0. Excel does not, so a sum that nearly cancels has no answer
/// there.
Value sum_of(const Call &call, const std::vector<double> &numbers) {
  double total = 0;
  double largest = 0;
  for (const double number : numbers) {
    total =
        is_libreoffice(call) ? approximate_add(total, number) : total + number;
    largest = std::max(largest, std::abs(number));
  }
  if (!is_libreoffice(call) && total != 0 &&
      std::abs(total) < largest * 1e-14) {
    throw NoAnswer{};
  }
  return checked(total);
}

Value sum(const Call &call) {
  const Collected collected = collect(call);
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  return sum_of(call, collected.numbers);
}

Value average(const Call &call) {
  const Collected collected = collect(call);
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  if (collected.numbers.empty()) {
    return Value{ErrorType::division};
  }
  const Value total = sum_of(call, collected.numbers);
  if (!total.holds<double>()) {
    return total;
  }
  return checked(total.get<double>() /
                 static_cast<double>(collected.numbers.size()));
}

template <bool largest> Value extreme(const Call &call) {
  const Collected collected = collect(call);
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  if (collected.numbers.empty()) {
    return Value{0.0};
  }
  return Value{largest ? std::ranges::max(collected.numbers)
                       : std::ranges::min(collected.numbers)};
}

Value product(const Call &call) {
  const Collected collected = collect(call);
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  if (collected.numbers.empty()) {
    return Value{0.0};
  }
  double result = 1;
  for (const double number : collected.numbers) {
    result *= number;
  }
  return checked(result);
}

Value sum_of_squares(const Call &call) {
  const Collected collected = collect(call);
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  double result = 0;
  for (const double number : collected.numbers) {
    result += number * number;
  }
  return checked(result);
}

/// The numbers of an aggregate that needs at least one, where none is
/// `#VALUE!` in LibreOffice and `#NUM!` in Excel.
std::optional<Value> no_numbers(const Call &call, const Collected &collected) {
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  if (collected.numbers.empty()) {
    return Value{is_libreoffice(call) ? ErrorType::value : ErrorType::number};
  }
  return std::nullopt;
}

Value median(const Call &call) {
  Collected collected = collect(call);
  if (const std::optional<Value> refused = no_numbers(call, collected)) {
    return *refused;
  }
  std::vector<double> &numbers = collected.numbers;
  std::ranges::sort(numbers);
  const std::size_t middle = numbers.size() / 2;
  if (numbers.size() % 2 == 1) {
    return Value{numbers[middle]};
  }
  return checked((numbers[middle - 1] + numbers[middle]) / 2);
}

/// The @p k th largest or smallest of the first argument. LibreOffice reads a
/// fraction of k up, Excel's rule for one is not documented.
template <bool largest> Value kth(const Call &call) {
  expect_arguments(call, 2, 2);
  Collected collected = collect(call, 0, 1);
  if (const std::optional<Value> refused = no_numbers(call, collected)) {
    return *refused;
  }
  const Number k = argument(call, 1);
  if (const auto *error = std::get_if<ErrorType>(&k)) {
    return Value{*error};
  }
  double rank = std::get<double>(k);
  if (rank != std::trunc(rank)) {
    if (!is_libreoffice(call)) {
      throw NoAnswer{};
    }
    rank = std::ceil(rank);
  }
  std::vector<double> &numbers = collected.numbers;
  if (rank < 1 || rank > static_cast<double>(numbers.size())) {
    return invalid(call);
  }
  std::ranges::sort(numbers);
  const auto at = static_cast<std::size_t>(rank) - 1;
  return Value{largest ? numbers[numbers.size() - 1 - at] : numbers[at]};
}

/// The variance of the numbers, of a sample where @p sample.
Value variance_of(const Call &call, const bool sample, const bool root) {
  const Collected collected = collect(call);
  if (collected.error.has_value()) {
    return Value{*collected.error};
  }
  const std::vector<double> &numbers = collected.numbers;
  const std::size_t least = sample ? 2 : 1;
  if (numbers.size() < least) {
    return Value{ErrorType::division};
  }
  double mean = 0;
  for (const double number : numbers) {
    mean += number;
  }
  mean /= static_cast<double>(numbers.size());
  double squares = 0;
  for (const double number : numbers) {
    squares += (number - mean) * (number - mean);
  }
  const double variance =
      squares / static_cast<double>(numbers.size() - (sample ? 1 : 0));
  return checked(root ? std::sqrt(variance) : variance);
}

Value count(const Call &call) {
  double result = 0;
  for (std::size_t i = 0; i < call.size(); ++i) {
    if (call.missing(i)) {
      throw NoAnswer{};
    }
    const Value value = call.value(i);
    const auto counts = [&](const Value &cell) {
      return cell.holds<double>() ||
             (cell.holds<bool>() && is_libreoffice(call));
    };
    if (const auto *reference = std::get_if<Reference>(&value.content)) {
      call.for_each(*reference, [&](const SheetPosition &, const Value &cell) {
        result += counts(cell) ? 1 : 0;
      });
    } else if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      result +=
          static_cast<double>(std::ranges::count_if(matrix->cells, counts));
    } else if (const auto *text = std::get_if<std::string>(&value.content)) {
      const std::optional<Number> read = number_of_text(*text);
      if (!read.has_value()) {
        throw NoAnswer{};
      }
      result += std::holds_alternative<double>(*read) ? 1 : 0;
    } else {
      result += value.holds<double>() || value.holds<bool>() ? 1 : 0;
    }
  }
  return Value{result};
}

Value count_stated(const Call &call) {
  double result = 0;
  for (std::size_t i = 0; i < call.size(); ++i) {
    if (call.missing(i)) {
      throw NoAnswer{};
    }
    const Value value = call.value(i);
    if (const auto *reference = std::get_if<Reference>(&value.content)) {
      call.for_each(*reference, [&](const SheetPosition &, const Value &cell) {
        result += cell.holds<Empty>() ? 0 : 1;
      });
    } else if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      result += static_cast<double>(
          std::ranges::count_if(matrix->cells, [](const Value &cell) {
            return !cell.holds<Empty>();
          }));
    } else {
      result += 1;
    }
  }
  return Value{result};
}

/// Empty cells, and in Excel the cells holding an empty text. Whether
/// LibreOffice counts an empty text depends on whether a formula made it.
Value count_blank(const Call &call) {
  expect_arguments(call, 1, 1);
  const Value value = call.value(0);
  const auto *reference = std::get_if<Reference>(&value.content);
  if (reference == nullptr) {
    throw NoAnswer{};
  }
  // the cells past the extent of the sheet are empty, and not visited
  double cells = 0;
  for (const Area &area : reference->areas) {
    cells +=
        (static_cast<double>(area.range.to().column) -
         area.range.from().column + 1) *
        (static_cast<double>(area.range.to().row) - area.range.from().row + 1);
  }
  double stated = 0;
  call.for_each(*reference, [&](const SheetPosition &, const Value &cell) {
    const auto *text = std::get_if<std::string>(&cell.content);
    if (text != nullptr && text->empty()) {
      if (is_libreoffice(call)) {
        throw NoAnswer{};
      }
      return;
    }
    stated += cell.holds<Empty>() ? 0 : 1;
  });
  return Value{cells - stated};
}

/// `SUMPRODUCT`: the arrays element by element. A text and an empty cell are
/// 0, and so is a boolean in Excel.
Value sum_product(const Call &call) {
  expect_arguments(call, 1, 255);
  std::vector<Matrix> arrays;
  for (std::size_t i = 0; i < call.size(); ++i) {
    arrays.push_back(call.array(i));
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
  std::optional<ErrorType> error;
  errors.into(error);
  if (error.has_value()) {
    return Value{*error};
  }
  return sum_of(call, products);
}

Value absolute(const Call &, const double x) { return Value{std::abs(x)}; }

Value sign(const Call &, const double x) {
  return Value{x > 0 ? 1.0 : x < 0 ? -1.0 : 0.0};
}

Value integer(const Call &, const double x) {
  return checked(stable(x, [](const double y) { return std::floor(y); }));
}

Value round_function(const Call &, const double x, const double digits) {
  return round_to(x, digits, round_half_away);
}
Value round_up(const Call &, const double x, const double digits) {
  return round_to(x, digits, round_away);
}
Value round_down(const Call &, const double x, const double digits) {
  return round_to(x, digits, round_toward);
}

Value modulo(const Call &call, const double x, const double y) {
  if (y == 0) {
    return Value{ErrorType::division};
  }
  const double quotient = x / y;
  // Excel refuses a quotient this large in some versions
  if (!is_libreoffice(call) && std::abs(quotient) >= 134217728.0) {
    throw NoAnswer{};
  }
  const double floor =
      stable(quotient, [](const double q) { return std::floor(q); });
  return checked(approximate_add(x, -y * floor));
}

Value quotient(const Call &call, const double x, const double y) {
  if (y == 0) {
    if (is_libreoffice(call)) {
      throw NoAnswer{};
    }
    return Value{ErrorType::division};
  }
  return checked(stable(x / y, round_toward));
}

Value square_root(const Call &call, const double x) {
  if (x < 0) {
    return invalid(call);
  }
  return Value{std::sqrt(x)};
}

Value exponential(const Call &, const double x) { return checked(std::exp(x)); }

Value natural_logarithm(const Call &call, const double x) {
  if (x <= 0) {
    return invalid(call);
  }
  return Value{std::log(x)};
}

Value logarithm(const Call &call, const double x, const double base) {
  if (x <= 0 || base <= 0) {
    return invalid(call);
  }
  if (base == 1) {
    if (is_libreoffice(call)) {
      throw NoAnswer{};
    }
    return Value{ErrorType::division};
  }
  return checked(std::log(x) / std::log(base));
}

Value logarithm_10(const Call &call, const double x) {
  if (x <= 0) {
    return invalid(call);
  }
  return Value{std::log10(x)};
}

Value power_function(const Call &call, const double x, const double y) {
  return power(x, y, call.settings().dialect);
}

Value pi(const Call &call) {
  expect_arguments(call, 0, 0);
  return Value{std::numbers::pi};
}

Value radians(const Call &, const double x) {
  return checked(x * std::numbers::pi / 180);
}
Value degrees(const Call &, const double x) {
  return checked(x * 180 / std::numbers::pi);
}
Value sine(const Call &, const double x) { return checked(std::sin(x)); }
Value cosine(const Call &, const double x) { return checked(std::cos(x)); }
Value tangent(const Call &, const double x) { return checked(std::tan(x)); }
Value hyperbolic_sine(const Call &, const double x) {
  return checked(std::sinh(x));
}
Value hyperbolic_cosine(const Call &, const double x) {
  return checked(std::cosh(x));
}
Value hyperbolic_tangent(const Call &, const double x) {
  return checked(std::tanh(x));
}
Value arc_tangent(const Call &, const double x) { return Value{std::atan(x)}; }

Value arc_sine(const Call &, const double x) {
  if (x < -1 || x > 1) {
    return Value{ErrorType::number};
  }
  return Value{std::asin(x)};
}

Value arc_cosine(const Call &, const double x) {
  if (x < -1 || x > 1) {
    return Value{ErrorType::number};
  }
  return Value{std::acos(x)};
}

/// `ATAN2(x; y)`: the angle of the point (x, y), which both applications
/// take in this order.
Value arc_tangent_2(const Call &call, const double x, const double y) {
  if (x == 0 && y == 0) {
    return is_libreoffice(call) ? Value{0.0} : Value{ErrorType::division};
  }
  return Value{std::atan2(y, x)};
}

/// `EVEN` and `ODD`: away from 0 to the next even or odd integer.
template <bool odd> Value parity(const Call &, const double x) {
  const double away = stable(x, round_away);
  const double magnitude = std::abs(away);
  double result = magnitude;
  if (odd) {
    result = std::fmod(magnitude, 2) == 1 ? magnitude : magnitude + 1;
  } else {
    result = std::fmod(magnitude, 2) == 0 ? magnitude : magnitude + 1;
  }
  return checked(x < 0 ? -result : result);
}

Value factorial(const Call &call, const double x) {
  const double n = stable(x, round_toward);
  if (n < 0) {
    return invalid(call);
  }
  if (n > 170) {
    return Value{is_libreoffice(call) ? ErrorType::value : ErrorType::number};
  }
  double result = 1;
  for (double i = 2; i <= n; ++i) {
    result *= i;
  }
  return Value{result};
}

constexpr FunctionEntry entries[] = {
    {"ABS", unary<absolute>},
    {"ACOS", unary<arc_cosine>},
    {"ASIN", unary<arc_sine>},
    {"ATAN", unary<arc_tangent>},
    {"ATAN2", binary<arc_tangent_2>},
    {"AVERAGE", average},
    {"COS", unary<cosine>},
    {"COSH", unary<hyperbolic_cosine>},
    {"COUNT", count},
    {"COUNTA", count_stated},
    {"COUNTBLANK", count_blank},
    {"DEGREES", unary<degrees>},
    {"EVEN", unary<parity<false>>},
    {"EXP", unary<exponential>},
    {"FACT", unary<factorial>},
    {"INT", unary<integer>},
    {"LARGE", kth<true>},
    {"LN", unary<natural_logarithm>},
    {"LOG", binary<logarithm, 10, true>},
    {"LOG10", unary<logarithm_10>},
    {"MAX", extreme<true>},
    {"MEDIAN", median},
    {"MIN", extreme<false>},
    {"MOD", binary<modulo>},
    {"ODD", unary<parity<true>>},
    {"PI", pi},
    {"POWER", binary<power_function>},
    {"PRODUCT", product},
    {"QUOTIENT", binary<quotient>},
    {"RADIANS", unary<radians>},
    {"ROUND", binary<round_function, 0, true>},
    {"ROUNDDOWN", binary<round_down, 0, true>},
    {"ROUNDUP", binary<round_up, 0, true>},
    {"SIGN", unary<sign>},
    {"SIN", unary<sine>},
    {"SINH", unary<hyperbolic_sine>},
    {"SMALL", kth<false>},
    {"SQRT", unary<square_root>},
    {"STDEV", [](const Call &call) { return variance_of(call, true, true); }},
    {"STDEVP", [](const Call &call) { return variance_of(call, false, true); }},
    {"SUM", sum},
    {"SUMPRODUCT", sum_product},
    {"SUMSQ", sum_of_squares},
    {"TAN", unary<tangent>},
    {"TANH", unary<hyperbolic_tangent>},
    {"TRUNC", binary<round_down, 0, true>},
    {"VAR", [](const Call &call) { return variance_of(call, true, false); }},
    {"VARP", [](const Call &call) { return variance_of(call, false, false); }},
};

} // namespace

std::span<const FunctionEntry> math_functions() { return entries; }

} // namespace odr::internal::formula
