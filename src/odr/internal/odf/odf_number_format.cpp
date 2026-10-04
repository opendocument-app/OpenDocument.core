#include <odr/internal/odf/odf_number_format.hpp>

#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace odr::internal::odf {

namespace {

std::string quoted(const std::string_view text) {
  std::string result = "\"";
  for (const char c : text) {
    result += c == '"' ? std::string("\"\\\"\"") : std::string(1, c);
  }
  return result + "\"";
}

bool is_long(const pugi::xml_node node) {
  return std::strcmp(node.attribute("number:style").value(), "long") == 0;
}

std::string repeated(const char c, const std::int32_t count) {
  return std::string(static_cast<std::size_t>(std::max<std::int32_t>(count, 0)),
                     c);
}

/// `number:number`: no `number:decimal-places` and no grouping is `General`,
/// as LibreOffice reads it.
std::optional<std::string> number_code(const pugi::xml_node node) {
  const std::optional<double> factor = util::number::parse(
      node.attribute("number:display-factor").as_string("1"));
  if (!factor.has_value() || !std::isfinite(*factor) || *factor <= 0) {
    return std::nullopt;
  }
  const pugi::xml_attribute decimals_attribute =
      node.attribute("number:decimal-places");
  const bool grouping = node.attribute("number:grouping").as_bool();
  if (!decimals_attribute && !grouping) {
    return *factor == 1 ? std::optional<std::string>("General") : std::nullopt;
  }
  const std::int32_t decimals = decimals_attribute.as_int();
  const std::int32_t min_decimals =
      node.attribute("number:min-decimal-places").as_int(decimals);
  const std::int32_t min_integer =
      node.attribute("number:min-integer-digits").as_int(1);

  // a comma between integer placeholders groups the whole integer
  const std::string zeros =
      min_integer == 0 ? std::string("#") : repeated('0', min_integer);
  std::string result = grouping ? "#,##" + zeros : zeros;
  if (decimals > 0) {
    result += "." + repeated('0', min_decimals) +
              repeated('#', decimals - min_decimals);
  }
  // a factor of 1000 is one scaling comma
  double remaining = *factor;
  while (remaining >= 1000) {
    result += ',';
    remaining /= 1000;
  }
  return remaining == 1 ? std::optional(result) : std::nullopt;
}

/// The code of one data style, its maps left out. Nothing where a part has no
/// code.
std::optional<std::string> section_code(const pugi::xml_node style) {
  const bool percentage =
      std::strcmp(style.name(), "number:percentage-style") == 0;
  const bool elapsed =
      !style.attribute("number:truncate-on-overflow").as_bool(true);

  std::string result;
  for (const pugi::xml_node child : style.children()) {
    const std::string_view name = child.name();
    if (name == "number:number") {
      const auto code = number_code(child);
      if (!code.has_value()) {
        return std::nullopt;
      }
      result += *code;
    } else if (name == "number:scientific-number") {
      const std::int32_t decimals =
          child.attribute("number:decimal-places").as_int();
      result +=
          repeated('0', child.attribute("number:min-integer-digits").as_int(1));
      if (decimals > 0) {
        result += "." + repeated('0', decimals);
      }
      result +=
          "E+" +
          repeated('0',
                   child.attribute("number:min-exponent-digits").as_int(2));
    } else if (name == "number:fraction") {
      if (const pugi::xml_attribute integer =
              child.attribute("number:min-integer-digits")) {
        result +=
            (integer.as_int() == 0 ? "#" : repeated('0', integer.as_int())) +
            " ";
      }
      result +=
          repeated('?',
                   child.attribute("number:min-numerator-digits").as_int(1)) +
          "/";
      if (const pugi::xml_attribute denominator =
              child.attribute("number:denominator-value")) {
        result += denominator.value();
      } else {
        result += repeated(
            '?', child.attribute("number:min-denominator-digits").as_int(1));
      }
    } else if (name == "number:text") {
      // the parse drops a text of spaces alone, and LibreOffice writes no
      // empty one, so an empty one held a space
      const std::string text = child.first_child() ? child.text().get() : " ";
      // a percentage style spells its `%` as text, and it scales the value
      std::size_t start = 0;
      for (std::size_t at = text.find('%');
           percentage && at != std::string::npos; at = text.find('%', start)) {
        if (at > start) {
          result += quoted(std::string_view(text).substr(start, at - start));
        }
        result += '%';
        start = at + 1;
      }
      if (start < text.size()) {
        result += quoted(std::string_view(text).substr(start));
      }
    } else if (name == "number:currency-symbol") {
      result += quoted(child.text().get());
    } else if (name == "number:text-content") {
      result += '@';
    } else if (name == "number:day") {
      result += is_long(child) ? "dd" : "d";
    } else if (name == "number:month") {
      result += child.attribute("number:textual").as_bool()
                    ? (is_long(child) ? "mmmm" : "mmm")
                    : (is_long(child) ? "mm" : "m");
    } else if (name == "number:year") {
      result += is_long(child) ? "yyyy" : "yy";
    } else if (name == "number:day-of-week") {
      result += is_long(child) ? "dddd" : "ddd";
    } else if (name == "number:hours") {
      const std::string hours = is_long(child) ? "hh" : "h";
      result += elapsed ? "[" + hours + "]" : hours;
    } else if (name == "number:minutes") {
      result += is_long(child) ? "mm" : "m";
    } else if (name == "number:seconds") {
      result += is_long(child) ? "ss" : "s";
      if (const std::int32_t decimals =
              child.attribute("number:decimal-places").as_int();
          decimals > 0) {
        result += "." + repeated('0', decimals);
      }
    } else if (name == "number:am-pm") {
      result += "AM/PM";
    } else if (name != "style:text-properties" && name != "style:map") {
      return std::nullopt; // an era, a quarter, a week, a boolean
    }
  }
  return result;
}

/// `value()>=0` as `>=0`.
std::optional<std::string> condition_of(const pugi::xml_node map) {
  std::string_view condition = map.attribute("style:condition").value();
  if (!condition.starts_with("value()")) {
    return std::nullopt;
  }
  condition.remove_prefix(7);
  std::string result;
  for (const char c : condition) {
    if (c != ' ') {
      result += c;
    }
  }
  if (result.starts_with("!=")) {
    result.replace(0, 2, "<>");
  }
  return result;
}

} // namespace

std::optional<std::string> format_code(const pugi::xml_node data_style,
                                       const DataStyleLookup &lookup) {
  const std::optional<std::string> main = section_code(data_style);
  if (!main) {
    return std::nullopt;
  }

  std::vector<std::pair<std::string, std::string>> maps;
  for (const pugi::xml_node map : data_style.children("style:map")) {
    const std::optional<std::string> condition = condition_of(map);
    const pugi::xml_node mapped =
        lookup(map.attribute("style:apply-style-name").value());
    const std::optional<std::string> code =
        mapped ? section_code(mapped) : std::nullopt;
    if (!condition || !code) {
      return std::nullopt;
    }
    maps.emplace_back(*condition, *code);
  }

  // the two shapes LibreOffice writes for a sign are a format code's own
  // sections, which take the sign off a negative value
  if (maps.empty()) {
    return main;
  }
  if (maps.size() == 1 && maps[0].first == ">=0") {
    return maps[0].second + ";" + *main;
  }
  if (maps.size() == 2 && maps[0].first == ">0" && maps[1].first == "<0") {
    return maps[0].second + ";" + maps[1].second + ";" + *main;
  }
  if (maps.size() > 2) {
    return std::nullopt;
  }
  std::string result;
  for (const auto &[condition, code] : maps) {
    result += "[" + condition + "]" + code + ";";
  }
  return result + *main;
}

std::optional<std::string> data_style_locale(const pugi::xml_node data_style) {
  if (const pugi::xml_attribute tag =
          data_style.attribute("number:rfc-language-tag")) {
    return tag.value();
  }
  const pugi::xml_attribute language = data_style.attribute("number:language");
  if (!language) {
    return std::nullopt;
  }
  std::string result = language.value();
  for (const char *subtag : {"number:script", "number:country"}) {
    if (const pugi::xml_attribute attribute = data_style.attribute(subtag)) {
      result += '-';
      result += attribute.value();
    }
  }
  return result;
}

} // namespace odr::internal::odf
