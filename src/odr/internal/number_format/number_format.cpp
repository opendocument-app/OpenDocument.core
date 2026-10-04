#include <odr/internal/number_format/number_format.hpp>

#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <fmt/format.h>

namespace odr::internal::number_format {

namespace {

using Kind = Token::Kind;

/// A value rounded to a number of decimals, half away from zero, on its
/// 15-digit decimal spelling, as a spreadsheet rounds it.
struct Decimal final {
  std::string integer;  ///< without leading zeros, empty for zero
  std::string fraction; ///< exactly the decimals asked for
};

Decimal round_decimal(const double value, const std::size_t decimals) {
  Decimal result;
  result.fraction.assign(decimals, '0');
  if (value == 0) {
    return result;
  }
  // `d.dddddddddddddde±x`: 15 significant digits
  const std::string spelled = fmt::format("{:.14e}", value);
  std::string digits = spelled.substr(0, 1) + spelled.substr(2, 14);
  int point = std::stoi(spelled.substr(spelled.find('e') + 1)) + 1;

  const long cut = point + static_cast<long>(decimals);
  if (cut < 0) {
    return result;
  }
  if (cut < static_cast<long>(digits.size())) {
    const bool up = digits[static_cast<std::size_t>(cut)] >= '5';
    digits.resize(static_cast<std::size_t>(cut));
    if (up) {
      std::size_t at = digits.size();
      for (; at > 0 && digits[at - 1] == '9'; --at) {
        digits[at - 1] = '0';
      }
      if (at == 0) {
        digits.insert(digits.begin(), '1');
        ++point;
      } else {
        ++digits[at - 1];
      }
    }
  }

  for (long i = 0; i < point; ++i) {
    result.integer += i < static_cast<long>(digits.size())
                          ? digits[static_cast<std::size_t>(i)]
                          : '0';
  }
  for (std::size_t i = 0; i < decimals; ++i) {
    const long at = point + static_cast<long>(i);
    if (at >= 0 && at < static_cast<long>(digits.size())) {
      result.fraction[i] = digits[static_cast<std::size_t>(at)];
    }
  }
  result.integer.erase(0, result.integer.find_first_not_of('0'));
  return result;
}

bool is_zero(const Decimal &decimal) {
  return decimal.integer.empty() &&
         decimal.fraction.find_first_not_of('0') == std::string::npos;
}

void append_literal(std::vector<Token> &tokens, const std::string &text) {
  if (!tokens.empty() && tokens.back().kind == Kind::literal) {
    tokens.back().text += text;
  } else {
    tokens.push_back({.kind = Kind::literal, .text = text});
  }
}

/// The content of `[…]`: a condition, a currency or locale, or a colour.
void parse_bracket(const std::string_view content, Section &section) {
  if (content.empty()) {
    throw std::invalid_argument("empty bracket in a format code");
  }
  if (content.front() == '<' || content.front() == '>' ||
      content.front() == '=') {
    Condition condition;
    std::size_t length = 1;
    if (content.starts_with("<=")) {
      condition.op = Condition::Operator::less_equal;
      length = 2;
    } else if (content.starts_with(">=")) {
      condition.op = Condition::Operator::greater_equal;
      length = 2;
    } else if (content.starts_with("<>")) {
      condition.op = Condition::Operator::not_equal;
      length = 2;
    } else if (content.front() == '<') {
      condition.op = Condition::Operator::less;
    } else if (content.front() == '>') {
      condition.op = Condition::Operator::greater;
    }
    const std::optional<double> operand =
        util::number::parse(content.substr(length));
    if (!operand) {
      throw std::invalid_argument("condition without a number");
    }
    condition.operand = *operand;
    section.condition = condition;
    return;
  }
  if (content.front() == '$') {
    // `[$€-407]`: the symbol is shown, the locale is not
    const std::string_view symbol = content.substr(1, content.find('-') - 1);
    if (!symbol.empty()) {
      append_literal(section.tokens, std::string(symbol));
    }
    return;
  }
  const char first = static_cast<char>(std::tolower(content.front()));
  if ((first == 'h' || first == 'm' || first == 's') &&
      std::ranges::all_of(content, [first](const char c) {
        return std::tolower(c) == first;
      })) {
    section.tokens.push_back({.kind = Kind::date_time,
                              .unit = first,
                              .width = content.size(),
                              .elapsed = true});
    return;
  }
  // a colour, `[DBNum1]` and the like change nothing here
}

/// `m` and `mm` are minutes after an hour or before a second, months
/// otherwise.
void resolve_minutes(Section &section) {
  std::vector<Token *> parts;
  for (Token &token : section.tokens) {
    if (token.kind == Kind::date_time && token.unit != 'a' &&
        token.unit != 'f') {
      parts.push_back(&token);
    }
  }
  for (std::size_t k = 0; k < parts.size(); ++k) {
    if (parts[k]->unit != 'M' || parts[k]->width > 2) {
      continue;
    }
    if ((k > 0 && parts[k - 1]->unit == 'h') ||
        (k + 1 < parts.size() && parts[k + 1]->unit == 's')) {
      parts[k]->unit = 'm';
    }
  }
}

std::vector<Section> parse_code(const std::string_view code) {
  std::vector<Section> sections(1);
  for (std::size_t i = 0; i < code.size(); ++i) {
    Section &section = sections.back();
    std::vector<Token> &tokens = section.tokens;
    const char c = code[i];
    switch (c) {
    case ';':
      sections.emplace_back();
      break;
    case '"': {
      const std::size_t end = code.find('"', i + 1);
      if (end == std::string_view::npos) {
        throw std::invalid_argument("unterminated quote in a format code");
      }
      append_literal(tokens, std::string(code.substr(i + 1, end - i - 1)));
      i = end;
    } break;
    case '\\':
      if (i + 1 < code.size()) {
        append_literal(tokens, std::string(1, code[++i]));
      }
      break;
    case '_':
      ++i; // the width of the next character, as a space
      append_literal(tokens, " ");
      break;
    case '*':
      ++i; // a fill, which needs the width of the cell
      break;
    case '[': {
      const std::size_t end = code.find(']', i + 1);
      if (end == std::string_view::npos) {
        throw std::invalid_argument("unterminated bracket in a format code");
      }
      parse_bracket(code.substr(i + 1, end - i - 1), section);
      i = end;
    } break;
    case '0':
    case '#':
    case '?':
      tokens.push_back({.kind = Kind::digit, .placeholder = c});
      break;
    case '.':
      if (!tokens.empty() && tokens.back().kind == Kind::date_time &&
          tokens.back().unit == 's' && i + 1 < code.size() &&
          code[i + 1] == '0') {
        std::size_t width = 0;
        for (; i + 1 < code.size() && code[i + 1] == '0'; ++i) {
          ++width;
        }
        tokens.push_back(
            {.kind = Kind::date_time, .unit = 'f', .width = width});
      } else {
        tokens.push_back({.kind = Kind::point});
      }
      break;
    case ',':
      tokens.push_back({.kind = Kind::comma});
      break;
    case '%':
      tokens.push_back({.kind = Kind::percent});
      break;
    case '/':
      tokens.push_back({.kind = Kind::slash});
      break;
    case '@':
      tokens.push_back({.kind = Kind::text});
      break;
    case 'E':
    case 'e':
      if (i + 1 < code.size() && (code[i + 1] == '+' || code[i + 1] == '-')) {
        tokens.push_back({.kind = Kind::exponent, .plus = code[++i] == '+'});
      } else {
        append_literal(tokens, std::string(1, c));
      }
      break;
    case 'G':
    case 'g':
      if (code.size() - i >= 7 &&
          std::ranges::equal(code.substr(i, 7), std::string_view("general"),
                             [](const char a, const char b) {
                               return std::tolower(a) == b;
                             })) {
        tokens.push_back({.kind = Kind::general});
        i += 6;
      } else {
        append_literal(tokens, std::string(1, c));
      }
      break;
    case 'y':
    case 'Y':
    case 'm':
    case 'M':
    case 'd':
    case 'D':
    case 'h':
    case 'H':
    case 's':
    case 'S': {
      const char letter = static_cast<char>(std::tolower(c));
      std::size_t width = 1;
      for (; i + 1 < code.size() && std::tolower(code[i + 1]) == letter; ++i) {
        ++width;
      }
      // `m` is the month until `resolve_minutes` says otherwise
      tokens.push_back({.kind = Kind::date_time,
                        .unit = letter == 'm' ? 'M' : letter,
                        .width = width});
    } break;
    case 'A':
    case 'a': {
      const auto spelled = [&](const std::string_view word) {
        return code.size() - i >= word.size() &&
               std::ranges::equal(code.substr(i, word.size()), word,
                                  [](const char a, const char b) {
                                    return std::tolower(a) == b;
                                  });
      };
      const std::size_t length = spelled("am/pm") ? 5 : spelled("a/p") ? 3 : 0;
      if (length == 0) {
        append_literal(tokens, std::string(1, c));
        break;
      }
      tokens.push_back({.kind = Kind::date_time,
                        .text = std::string(code.substr(i, length)),
                        .unit = 'a'});
      i += length - 1;
    } break;
    default:
      append_literal(tokens, std::string(1, c));
      break;
    }
  }
  for (Section &section : sections) {
    resolve_minutes(section);
  }
  return sections;
}

bool has(const Section &section, const Kind kind) {
  return std::ranges::any_of(section.tokens, [kind](const Token &token) {
    return token.kind == kind;
  });
}

/// The digits of @p digits put onto @p placeholders, one piece each, right to
/// left; the leftmost takes what is left over.
std::vector<std::string> place_integer(const std::string &digits,
                                       const std::string &placeholders) {
  const std::size_t count = placeholders.size();
  std::vector<std::string> pieces(count);
  for (std::size_t j = 0; j < count; ++j) {
    std::string &piece = pieces[count - 1 - j];
    const char placeholder = placeholders[count - 1 - j];
    if (j < digits.size()) {
      piece = digits[digits.size() - 1 - j];
    } else if (placeholder == '0') {
      piece = "0";
    } else if (placeholder == '?') {
      piece = " ";
    }
  }
  if (count > 0 && digits.size() > count) {
    pieces.front().insert(0, digits.substr(0, digits.size() - count));
  }
  return pieces;
}

std::string join(const std::vector<std::string> &pieces) {
  std::string result;
  for (const std::string &piece : pieces) {
    result += piece;
  }
  return result;
}

std::string group(const std::string &integer, const std::string &separator) {
  std::string result;
  std::size_t seen = 0;
  for (std::size_t i = integer.size(); i > 0; --i) {
    const char c = integer[i - 1];
    if (c >= '0' && c <= '9') {
      if (seen > 0 && seen % 3 == 0) {
        result.insert(0, separator);
      }
      ++seen;
    }
    result.insert(result.begin(), c);
  }
  return result;
}

/// The plain part of a number: the tokens of @p tokens in [begin, end), the
/// integer ones before a point and the decimal ones after it.
std::string format_plain(const std::vector<Token> &tokens,
                         const std::size_t begin, const std::size_t end,
                         double value, const Symbols &symbols) {
  std::size_t point = end;
  std::size_t first_digit = end;
  std::size_t last_digit = end;
  std::size_t last_integer_digit = end;
  std::string integer_placeholders;
  std::string fraction_placeholders;
  for (std::size_t i = begin; i < end; ++i) {
    if (tokens[i].kind == Kind::point && point == end) {
      point = i;
    } else if (tokens[i].kind == Kind::digit) {
      first_digit = std::min(first_digit, i);
      last_digit = i;
      if (point == end) {
        integer_placeholders += tokens[i].placeholder;
        last_integer_digit = i;
      } else {
        fraction_placeholders += tokens[i].placeholder;
      }
    }
  }

  // a comma between integer placeholders groups, one after the last
  // placeholder divides by a thousand, and any other is a literal
  enum class Role { literal, grouping, scaling };
  std::vector<Role> roles(end, Role::literal);
  bool grouped = false;
  std::size_t scales = 0;
  for (std::size_t i = begin; i < end; ++i) {
    if (tokens[i].kind != Kind::comma || first_digit > i) {
      continue;
    }
    if (last_integer_digit != end && i < last_integer_digit) {
      roles[i] = Role::grouping;
      grouped = true;
    } else if (i > last_digit) {
      roles[i] = Role::scaling;
      ++scales;
    }
  }
  value /= std::pow(1000.0, static_cast<double>(scales));

  const Decimal decimal = round_decimal(value, fraction_placeholders.size());

  // trailing zeros a `#` drops and a `?` blanks
  std::vector<std::string> fraction(fraction_placeholders.size());
  bool trailing = true;
  for (std::size_t k = fraction.size(); k > 0; --k) {
    const char digit = decimal.fraction[k - 1];
    const char placeholder = fraction_placeholders[k - 1];
    trailing = trailing && digit == '0' && placeholder != '0';
    fraction[k - 1] = !trailing            ? std::string(1, digit)
                      : placeholder == '?' ? " "
                                           : "";
  }

  const std::vector<std::string> integer =
      place_integer(decimal.integer, integer_placeholders);
  const std::string grouped_integer =
      grouped ? group(join(integer), symbols.group) : "";

  std::string result;
  std::size_t integer_seen = 0;
  std::size_t fraction_seen = 0;
  for (std::size_t i = begin; i < end; ++i) {
    const Token &token = tokens[i];
    switch (token.kind) {
    case Kind::digit:
      if (i < point) {
        // grouped, the whole integer goes out at its last placeholder
        if (!grouped) {
          result += integer[integer_seen];
        } else if (integer_seen + 1 == integer.size()) {
          result += grouped_integer;
        }
        ++integer_seen;
      } else {
        result += fraction[fraction_seen++];
      }
      break;
    case Kind::point:
      if (integer_placeholders.empty()) {
        result += decimal.integer;
      }
      result += symbols.decimal;
      break;
    case Kind::literal:
      result += token.text;
      break;
    case Kind::percent:
      result += '%';
      break;
    case Kind::comma:
      if (roles[i] == Role::literal) {
        result += ',';
      }
      break;
    default:
      break;
    }
  }
  return result;
}

std::string format_scientific(const std::vector<Token> &tokens,
                              const std::size_t exponent, double value,
                              const Symbols &symbols) {
  std::size_t integer_count = 0;
  bool hashed = false;
  std::size_t point = exponent;
  for (std::size_t i = 0; i < exponent; ++i) {
    if (tokens[i].kind == Kind::point) {
      point = i;
    } else if (tokens[i].kind == Kind::digit && point == exponent) {
      ++integer_count;
      hashed = hashed || tokens[i].placeholder == '#';
    }
  }
  integer_count = std::max<std::size_t>(integer_count, 1);
  std::size_t fraction_count = 0;
  for (std::size_t i = point; i < exponent; ++i) {
    fraction_count += tokens[i].kind == Kind::digit ? 1 : 0;
  }

  int power = 0;
  if (value != 0) {
    power = static_cast<int>(std::floor(std::log10(value)));
    const int step = static_cast<int>(integer_count);
    if (hashed && step > 1) {
      power = static_cast<int>(std::floor(static_cast<double>(power) / step)) *
              step;
    } else {
      power -= step - 1;
    }
    // rounding may carry into one more digit, and so into the next power
    const Decimal decimal =
        round_decimal(value / std::pow(10.0, power), fraction_count);
    if (decimal.integer.size() > integer_count) {
      power += hashed && step > 1 ? step : 1;
    }
  }
  const double mantissa = value / std::pow(10.0, power);

  std::string exponent_digits;
  std::size_t end = exponent + 1;
  for (; end < tokens.size() && tokens[end].kind == Kind::digit; ++end) {
    exponent_digits += tokens[end].placeholder;
  }

  std::string result = format_plain(tokens, 0, exponent, mantissa, symbols);
  result += 'E';
  if (power < 0) {
    result += '-';
  } else if (tokens[exponent].plus) {
    result += '+';
  }
  std::string digits = std::to_string(std::abs(power));
  if (digits == "0" && !exponent_digits.empty()) {
    digits.clear();
  }
  result += join(
      place_integer(digits, exponent_digits.empty() ? "0" : exponent_digits));
  for (std::size_t i = end; i < tokens.size(); ++i) {
    if (tokens[i].kind == Kind::literal) {
      result += tokens[i].text;
    }
  }
  return result;
}

/// The best fraction of @p value whose denominator has at most @p digits
/// digits.
std::pair<std::int64_t, std::int64_t> approximate(const double value,
                                                  const std::size_t digits) {
  const auto most = static_cast<std::int64_t>(
      std::pow(10.0, static_cast<double>(std::min<std::size_t>(digits, 4))) -
      1);
  std::pair<std::int64_t, std::int64_t> best{std::llround(value), 1};
  double error = std::abs(value - static_cast<double>(best.first));
  for (std::int64_t denominator = 2; denominator <= most; ++denominator) {
    const std::int64_t numerator =
        std::llround(value * static_cast<double>(denominator));
    const double distance =
        std::abs(value - static_cast<double>(numerator) /
                             static_cast<double>(denominator));
    if (distance < error - 1e-12) {
      best = {numerator, denominator};
      error = distance;
    }
  }
  return best;
}

std::string pad(const std::string &digits, const std::string &placeholders,
                const bool right) {
  std::string result = digits;
  for (std::size_t i = digits.size(); i < placeholders.size(); ++i) {
    const char placeholder = placeholders[placeholders.size() - 1 - i];
    if (placeholder == '#') {
      continue;
    }
    const char fill = placeholder == '0' ? '0' : ' ';
    if (right && fill == ' ') {
      result += fill;
    } else {
      result.insert(result.begin(), fill);
    }
  }
  return result;
}

std::string format_fraction(const std::vector<Token> &tokens,
                            const std::size_t slash, const double value) {
  // the numerator is the run of placeholders right before the bar
  std::size_t numerator_begin = slash;
  while (numerator_begin > 0 &&
         tokens[numerator_begin - 1].kind == Kind::digit) {
    --numerator_begin;
  }
  std::string numerator_placeholders;
  for (std::size_t i = numerator_begin; i < slash; ++i) {
    numerator_placeholders += tokens[i].placeholder;
  }
  std::string integer_placeholders;
  for (std::size_t i = 0; i < numerator_begin; ++i) {
    if (tokens[i].kind == Kind::digit) {
      integer_placeholders += tokens[i].placeholder;
    }
  }

  // the denominator: placeholders, or digits that fix it
  std::size_t end = slash + 1;
  std::string denominator_placeholders;
  std::string fixed;
  for (; end < tokens.size(); ++end) {
    const Token &token = tokens[end];
    if (token.kind == Kind::digit) {
      denominator_placeholders += token.placeholder;
      fixed += token.placeholder == '0' ? "0" : "";
    } else if (token.kind == Kind::literal && !token.text.empty() &&
               std::ranges::all_of(token.text, [](const char c) {
                 return c >= '0' && c <= '9';
               })) {
      fixed += token.text;
      denominator_placeholders += std::string(token.text.size(), '0');
    } else {
      break;
    }
  }
  const bool fixed_denominator =
      fixed.find_first_not_of('0') != std::string::npos;

  double whole = 0;
  double part = value;
  if (!integer_placeholders.empty()) {
    whole = std::floor(value);
    part = value - whole;
  }
  std::int64_t numerator = 0;
  std::int64_t denominator = 1;
  if (fixed_denominator) {
    denominator = std::stoll(fixed);
    numerator = std::llround(part * static_cast<double>(denominator));
  } else {
    std::tie(numerator, denominator) =
        approximate(part, denominator_placeholders.size());
  }
  if (!integer_placeholders.empty() && numerator == denominator) {
    whole += 1;
    numerator = 0;
  }

  std::string fraction_text;
  if (numerator != 0 || integer_placeholders.empty()) {
    fraction_text =
        pad(std::to_string(numerator), numerator_placeholders, false) + "/" +
        (fixed_denominator ? std::to_string(denominator)
                           : pad(std::to_string(denominator),
                                 denominator_placeholders, true));
  } else {
    fraction_text.assign(numerator_placeholders.size() + 1 +
                             denominator_placeholders.size(),
                         ' ');
  }

  // the integer, then whatever separates it from the numerator
  std::string head;
  if (!integer_placeholders.empty()) {
    const std::string digits =
        whole == 0 ? std::string() : fmt::format("{:.0f}", whole);
    head = join(place_integer(digits.empty() && numerator == 0 ? "0" : digits,
                              integer_placeholders));
    std::string between;
    std::size_t last_integer = 0;
    for (std::size_t i = 0; i < numerator_begin; ++i) {
      if (tokens[i].kind == Kind::digit) {
        last_integer = i;
      }
    }
    for (std::size_t i = last_integer + 1; i < numerator_begin; ++i) {
      if (tokens[i].kind == Kind::literal) {
        between += tokens[i].text;
      }
    }
    if (head.find_first_not_of(' ') == std::string::npos) {
      between.assign(between.size(), ' ');
    }
    head += between;
  }
  std::string result = head + fraction_text;
  for (std::size_t i = end; i < tokens.size(); ++i) {
    if (tokens[i].kind == Kind::literal) {
      result += tokens[i].text;
    }
  }
  return result;
}

/// The civil date @p days after 1970-01-01, as Howard Hinnant's
/// `civil_from_days` computes it.
std::tuple<std::int64_t, std::uint32_t, std::uint32_t>
civil(std::int64_t days) {
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const auto day_of_era = static_cast<std::uint32_t>(days - era * 146097);
  const std::uint32_t year_of_era = (day_of_era - day_of_era / 1460 +
                                     day_of_era / 36524 - day_of_era / 146096) /
                                    365;
  const std::uint32_t day_of_year =
      day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
  const std::uint32_t shifted_month = (5 * day_of_year + 2) / 153;
  const std::uint32_t day = day_of_year - (153 * shifted_month + 2) / 5 + 1;
  const std::uint32_t month =
      shifted_month < 10 ? shifted_month + 3 : shifted_month - 9;
  const std::int64_t year =
      static_cast<std::int64_t>(year_of_era) + era * 400 + (month <= 2 ? 1 : 0);
  return {year, month, day};
}

constexpr std::array<std::string_view, 12> month_names{
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
constexpr std::array<std::string_view, 7> day_names{
    "Sunday",   "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"};

std::string padded(const std::int64_t value, const std::size_t width) {
  std::string digits = std::to_string(value);
  if (digits.size() < width) {
    digits.insert(0, width - digits.size(), '0');
  }
  return digits;
}

/// @p value as a date serial: whole days counted from @p epoch, and the time
/// of day in its fraction.
std::string format_date_time(const std::vector<Token> &tokens,
                             const double value, const Epoch epoch) {
  std::size_t fraction_width = 0;
  bool twelve_hours = false;
  for (const Token &token : tokens) {
    if (token.kind == Kind::date_time && token.unit == 'f') {
      fraction_width =
          std::max(fraction_width, std::min<std::size_t>(token.width, 3));
    }
    twelve_hours =
        twelve_hours || (token.kind == Kind::date_time && token.unit == 'a');
  }
  const auto scale = static_cast<std::int64_t>(
      std::pow(10.0, static_cast<double>(fraction_width)));
  const std::int64_t ticks =
      std::llround(value * 86400.0 * static_cast<double>(scale));
  const std::int64_t day = ticks / (86400 * scale);
  const std::int64_t second_of_day = ticks % (86400 * scale) / scale;
  const std::int64_t fraction = ticks % scale;
  const std::int64_t hour = second_of_day / 3600;

  // 1900 counts 1900-02-29, which never was, as day 60, so its weekdays only
  // agree with the calendar from day 61 on
  std::int64_t year = 1900;
  std::uint32_t month = 2;
  std::uint32_t month_day = 29;
  std::int64_t weekday = 0;
  if (epoch == Epoch::from_1904) {
    std::tie(year, month, month_day) = civil(day - 24107);
    weekday = ((day - 24107) % 7 + 11) % 7;
  } else {
    if (day != 60) {
      std::tie(year, month, month_day) =
          civil(day - (day < 60 ? 25568 : 25569));
    }
    weekday = (day + 6) % 7;
  }

  std::string result;
  for (const Token &token : tokens) {
    // the separators of a date are its literals
    if (token.kind == Kind::literal) {
      result += token.text;
    } else if (token.kind == Kind::slash) {
      result += '/';
    } else if (token.kind == Kind::comma) {
      result += ',';
    } else if (token.kind == Kind::point) {
      result += '.';
    }
    if (token.kind != Kind::date_time) {
      continue;
    }
    switch (token.unit) {
    case 'y':
      result += token.width <= 2 ? padded(year % 100, 2) : padded(year, 4);
      break;
    case 'M':
      result +=
          token.width <= 2   ? padded(month, token.width)
          : token.width == 3 ? std::string(month_names[month - 1].substr(0, 3))
          : token.width == 4 ? std::string(month_names[month - 1])
                             : std::string(month_names[month - 1].substr(0, 1));
      break;
    case 'd':
      result +=
          token.width <= 2 ? padded(month_day, token.width)
          : token.width == 3
              ? std::string(
                    day_names[static_cast<std::size_t>(weekday)].substr(0, 3))
              : std::string(day_names[static_cast<std::size_t>(weekday)]);
      break;
    case 'h':
      result += padded(token.elapsed  ? ticks / (3600 * scale)
                       : twelve_hours ? (hour % 12 == 0 ? 12 : hour % 12)
                                      : hour,
                       token.width);
      break;
    case 'm':
      result +=
          padded(token.elapsed ? ticks / (60 * scale) : second_of_day / 60 % 60,
                 token.width);
      break;
    case 's':
      result += padded(token.elapsed ? ticks / scale : second_of_day % 60,
                       token.width);
      break;
    case 'f':
      result += '.' + padded(fraction, fraction_width).substr(0, token.width);
      break;
    case 'a': {
      const bool morning = hour < 12;
      result += token.text.size() == 5 ? token.text.substr(morning ? 0 : 3, 2)
                                       : token.text.substr(morning ? 0 : 2, 1);
    } break;
    default:
      break;
    }
  }
  return result;
}

std::string format_section(const Section &section, const double value,
                           const Symbols &symbols) {
  const std::vector<Token> &tokens = section.tokens;
  double scaled = value;
  for (const Token &token : tokens) {
    if (token.kind == Kind::percent) {
      scaled *= 100;
    }
  }

  if (has(section, Kind::general)) {
    std::string result;
    for (const Token &token : tokens) {
      result += token.kind == Kind::general   ? format_general(scaled, symbols)
                : token.kind == Kind::literal ? token.text
                : token.kind == Kind::percent ? "%"
                                              : "";
    }
    return result;
  }

  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].kind == Kind::exponent) {
      return format_scientific(tokens, i, scaled, symbols);
    }
  }
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].kind == Kind::slash && i > 0 &&
        tokens[i - 1].kind == Kind::digit) {
      return format_fraction(tokens, i, scaled);
    }
  }
  if (!has(section, Kind::digit)) {
    // literals alone, as `"n/a"` or an empty section that hides the value
    std::string result;
    for (const Token &token : tokens) {
      if (token.kind == Kind::literal) {
        result += token.text;
      }
    }
    return result;
  }
  return format_plain(tokens, 0, tokens.size(), scaled, symbols);
}

/// Whether @p section rounds @p value to nothing it shows.
bool shows_zero(const Section &section, const double value) {
  std::size_t decimals = 0;
  bool after_point = false;
  for (const Token &token : section.tokens) {
    after_point = after_point || token.kind == Kind::point;
    decimals += after_point && token.kind == Kind::digit ? 1 : 0;
  }
  return is_zero(round_decimal(std::abs(value), decimals));
}

} // namespace

bool Condition::matches(const double value) const {
  switch (op) {
  case Operator::less:
    return value < operand;
  case Operator::less_equal:
    return value <= operand;
  case Operator::greater:
    return value > operand;
  case Operator::greater_equal:
    return value >= operand;
  case Operator::equal:
    return value == operand;
  case Operator::not_equal:
    return value != operand;
  }
  return false;
}

Format::Format() : m_sections{Section{{{.kind = Kind::general}}, {}}} {}

Format::Format(const std::string_view code) : m_sections{parse_code(code)} {
  if (m_sections.size() > 4) {
    throw std::invalid_argument("a format code has four sections at most");
  }
}

Category Format::category() const {
  Category result = Category::number;
  for (const Token &token : m_sections.front().tokens) {
    if (token.kind != Kind::date_time) {
      continue;
    }
    if (token.unit == 'y' || token.unit == 'M' || token.unit == 'd') {
      return Category::date;
    }
    result = Category::time;
  }
  return result;
}

std::string Format::format(const double value, const Epoch epoch,
                           const Symbols &symbols) const {
  if (!std::isfinite(value)) {
    return format_general(value, symbols);
  }
  // the text section is not a number's
  std::size_t count = std::min<std::size_t>(m_sections.size(), 3);
  while (count > 1 && has(m_sections[count - 1], Kind::text) &&
         !has(m_sections[count - 1], Kind::digit)) {
    --count;
  }
  const bool conditional = std::ranges::any_of(
      m_sections.begin(), m_sections.begin() + count,
      [](const Section &s) { return s.condition.has_value(); });

  // MS-XLS 2.4.126: one section for every number; two for the non-negative
  // ones and the negative ones; three for positive, negative and zero
  std::size_t chosen = 0;
  bool negative_section = false;
  if (conditional) {
    chosen = count - 1;
    for (std::size_t i = 0; i < count; ++i) {
      const std::optional<Condition> &condition = m_sections[i].condition;
      if (!condition || condition->matches(value)) {
        chosen = i;
        break;
      }
    }
    const std::optional<Condition> &condition = m_sections[chosen].condition;
    negative_section = condition &&
                       (condition->op == Condition::Operator::less ||
                        condition->op == Condition::Operator::less_equal) &&
                       condition->operand <= 0;
  } else if (count >= 3 && value == 0) {
    chosen = 2;
  } else if (count >= 2 && value < 0) {
    chosen = 1;
    negative_section = true;
  }

  const Section &section = m_sections[chosen];
  if (has(section, Kind::date_time)) {
    // a spreadsheet shows a date before its epoch or after 9999-12-31 as
    // `####`
    constexpr double last_serial = 2958465;
    return value < 0 || value >= last_serial + 1
               ? format_general(value, symbols)
               : format_date_time(section.tokens, value, epoch);
  }
  if (has(section, Kind::text) && !has(section, Kind::digit)) {
    return format_general(value, symbols);
  }
  std::string result = format_section(section, std::abs(value), symbols);
  const bool shows_nothing =
      has(section, Kind::general) ? value == 0 : shows_zero(section, value);
  if (value < 0 && !negative_section && !shows_nothing) {
    result.insert(result.begin(), '-');
  }
  return result;
}

std::string Format::format(const std::string_view value) const {
  const Section *section = nullptr;
  if (m_sections.size() == 4) {
    section = &m_sections[3];
  } else if (m_sections.size() == 1 && has(m_sections[0], Kind::text)) {
    section = &m_sections[0];
  }
  if (section == nullptr) {
    return std::string(value);
  }
  std::string result;
  for (const Token &token : section->tokens) {
    if (token.kind == Kind::text) {
      result += value;
    } else if (token.kind == Kind::literal) {
      result += token.text;
    }
  }
  return result;
}

} // namespace odr::internal::number_format

namespace odr::internal {

std::string number_format::format_general(const double value,
                                          const Symbols &symbols) {
  if (value == 0) {
    return "0";
  }
  if (!std::isfinite(value)) {
    return fmt::format("{}", value);
  }
  const double magnitude = std::abs(value);
  const std::string sign = value < 0 ? "-" : "";
  const auto trimmed = [&symbols](std::string text) {
    if (const std::size_t point = text.find('.'); point != std::string::npos) {
      text.erase(text.find_last_not_of('0') + 1);
      if (text.back() == '.') {
        text.pop_back();
      } else {
        text.replace(point, 1, symbols.decimal);
      }
    }
    return text;
  };
  if (magnitude >= 1e-10 && magnitude < 1e15) {
    const int power = static_cast<int>(std::floor(std::log10(magnitude)));
    const auto decimals = static_cast<std::size_t>(std::max(0, 14 - power));
    const Decimal decimal = round_decimal(magnitude, decimals);
    return sign + trimmed((decimal.integer.empty() ? "0" : decimal.integer) +
                          "." + decimal.fraction);
  }
  const std::string spelled = fmt::format("{:.14e}", magnitude);
  const std::size_t e = spelled.find('e');
  const int power = std::stoi(spelled.substr(e + 1));
  return sign + trimmed(spelled.substr(0, e)) + "E" + (power < 0 ? "-" : "+") +
         fmt::format("{:02d}", std::abs(power));
}

std::int64_t number_format::days_from_civil(std::int64_t year,
                                            const std::uint32_t month,
                                            const std::uint32_t day) {
  // Howard Hinnant's `days_from_civil`, moved from 1970-01-01 to 1899-12-30
  year -= month <= 2 ? 1 : 0;
  const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
  const auto year_of_era = static_cast<std::uint32_t>(year - era * 400);
  const std::uint32_t day_of_year =
      (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  const std::uint32_t day_of_era =
      year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return era * 146097 + static_cast<std::int64_t>(day_of_era) - 719468 + 25569;
}

double number_format::days_from_serial(const double serial, const Epoch epoch) {
  if (epoch == Epoch::from_1904) {
    return serial + 1462;
  }
  return serial < 61 ? serial + 1 : serial;
}

double number_format::serial_from_days(const double days, const Epoch epoch) {
  if (epoch == Epoch::from_1904) {
    return days - 1462;
  }
  return days < 61 ? days - 1 : days;
}

number_format::Symbols
number_format::symbols_of(const std::string_view locale) {
  const std::string_view language = locale.substr(0, locale.find('-'));
  const std::string_view region = locale.find('-') == std::string_view::npos
                                      ? std::string_view()
                                      : locale.substr(locale.rfind('-') + 1);
  // CLDR, for the languages a spreadsheet is most often written in
  constexpr std::array<std::string_view, 16> point_group{
      "de", "es", "it", "nl", "pt", "id", "tr", "da",
      "el", "ro", "sl", "hr", "sr", "bs", "vi", "ca"};
  constexpr std::array<std::string_view, 18> space_group{
      "fr", "ru", "pl", "cs", "sk", "sv", "fi", "nb", "nn",
      "no", "uk", "hu", "bg", "lt", "lv", "et", "be", "kk"};
  if ((language == "de" || language == "it") &&
      (region == "CH" || region == "LI")) {
    return {".", "\u2019"};
  }
  if (language == "es" && (region == "MX" || region == "US")) {
    return {".", ","};
  }
  if (language == "pt" && region == "PT") {
    return {",", "\u00a0"};
  }
  if (std::ranges::find(point_group, language) != std::end(point_group)) {
    return {",", "."};
  }
  if (std::ranges::find(space_group, language) != std::end(space_group)) {
    return {",", language == "fr" ? "\u202f" : "\u00a0"};
  }
  return {};
}

} // namespace odr::internal
