#include <odr/internal/util/locale_util.hpp>

#include <odr/internal/util/lowercase_table.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <span>

namespace odr::internal {

util::locale::LanguageTag
util::locale::LanguageTag::parse(const std::string_view tag) {
  LanguageTag result;
  std::string_view rest = tag;
  for (bool first = true; !rest.empty(); first = false) {
    const std::size_t end = rest.find_first_of("-_");
    const std::string_view subtag = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view()
                                         : rest.substr(end + 1);
    const bool letters =
        !subtag.empty() &&
        std::ranges::all_of(subtag, util::string::is_ascii_letter);
    const bool digits =
        !subtag.empty() &&
        std::ranges::all_of(subtag, util::string::is_ascii_digit);
    if (first) {
      result.language = util::string::to_lower(subtag);
      // `no` is Norwegian, which is written as Bokmål
      if (result.language == "no") {
        result.language = "nb";
      }
    } else if (subtag.size() == 1) {
      break; // an extension or a private use follows
    } else if (subtag.size() == 4 && letters && result.script.empty() &&
               result.region.empty()) {
      result.script = util::string::to_lower(subtag);
      result.script.front() = util::string::to_upper(result.script.front());
    } else if (((subtag.size() == 2 && letters) ||
                (subtag.size() == 3 && digits)) &&
               result.region.empty()) {
      for (const char c : subtag) {
        result.region += util::string::to_upper(c);
      }
    }
  }
  return result;
}

char32_t util::locale::to_lower(const char32_t letter,
                                const std::string_view language) {
  if (letter == U'I' && (language == "tr" || language == "az")) {
    return U'ı';
  }
  const std::span<const LowercasePair> pairs = lowercase_pairs();
  const auto found =
      std::ranges::lower_bound(pairs, letter, {}, &LowercasePair::letter);
  return found != pairs.end() && found->letter == letter ? found->lower
                                                         : letter;
}

bool util::locale::equals_ignore_case(std::string_view a, std::string_view b,
                                      const std::string_view language) {
  while (!a.empty() && !b.empty()) {
    if (to_lower(util::string::next_utf8(a), language) !=
        to_lower(util::string::next_utf8(b), language)) {
      return false;
    }
  }
  return a.empty() && b.empty();
}

} // namespace odr::internal
