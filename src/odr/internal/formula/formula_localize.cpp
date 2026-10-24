#include <odr/internal/formula/formula_function_names.hpp>

#include <odr/internal/util/string_util.hpp>

#include <optional>

namespace odr::internal::formula {

namespace {

/// The language of @p locale as LibreOffice names its translations.
std::string language_of(const std::string_view locale) {
  if (util::string::equals_ignore_case(locale, "pt-BR")) {
    return "pt-BR";
  }
  std::string language =
      util::string::to_lower(locale.substr(0, locale.find('-')));
  // `no` is Norwegian, which is written as Bokmål
  return language == "no" ? "nb" : language;
}

/// What a function name is spelled out of: a byte of a letter past ASCII
/// too, as `ZÄHLENWENN` holds.
bool is_name_byte(const char c) {
  return util::string::is_ascii_letter_or_digit(c) || c == '_' || c == '.' ||
         static_cast<unsigned char>(c) >= 0x80;
}

/// @p formula with each name ahead of `(` that @p replace knows replaced,
/// outside a text and a quoted sheet name.
template <typename Replace>
std::string replaced(const std::string_view formula, const Replace &replace) {
  std::string result;
  result.reserve(formula.size());
  char quote = '\0';
  std::size_t i = 0;
  while (i < formula.size()) {
    const char c = formula[i];
    if (quote != '\0') {
      quote = c == quote ? '\0' : quote;
      result += c;
      ++i;
    } else if (c == '"' || c == '\'') {
      quote = c;
      result += c;
      ++i;
    } else if (is_name_byte(c)) {
      std::size_t end = i;
      while (end < formula.size() && is_name_byte(formula[end])) {
        ++end;
      }
      const std::string_view name = formula.substr(i, end - i);
      std::size_t next = end;
      while (next < formula.size() && formula[next] == ' ') {
        ++next;
      }
      const std::optional<std::string_view> other =
          next < formula.size() && formula[next] == '('
              ? replace(name)
              : std::optional<std::string_view>();
      result += other.has_value() ? *other : name;
      i = end;
    } else {
      result += c;
      ++i;
    }
  }
  return result;
}

} // namespace

std::string localized_functions(const std::string_view formula,
                                const std::string_view locale) {
  const std::string language = language_of(locale);
  return replaced(
      formula,
      [&](const std::string_view name) -> std::optional<std::string_view> {
        for (const FunctionName &entry : function_names()) {
          if (entry.language == language &&
              util::string::equals_ignore_case(entry.english, name)) {
            return entry.localized;
          }
        }
        return std::nullopt;
      });
}

std::string english_functions(const std::string_view formula,
                              const std::string_view locale) {
  const std::string language = language_of(locale);
  return replaced(
      formula,
      [&](const std::string_view name) -> std::optional<std::string_view> {
        for (const FunctionName &entry : function_names()) {
          if (entry.language == language &&
              util::string::equals_ignore_case(entry.localized, name)) {
            return entry.english;
          }
        }
        return std::nullopt;
      });
}

} // namespace odr::internal::formula
