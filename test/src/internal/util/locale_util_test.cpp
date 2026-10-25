#include <odr/internal/util/locale_util.hpp>

#include <gtest/gtest.h>

using namespace odr::internal::util::locale;

namespace {

void expect_tag(const std::string_view tag, const std::string_view language,
                const std::string_view script, const std::string_view region) {
  SCOPED_TRACE(tag);
  const LanguageTag parsed = LanguageTag::parse(tag);
  EXPECT_EQ(parsed.language, language);
  EXPECT_EQ(parsed.script, script);
  EXPECT_EQ(parsed.region, region);
}

} // namespace

TEST(LocaleUtil, a_tag_states_its_language_and_region) {
  expect_tag("de", "de", "", "");
  expect_tag("de-DE", "de", "", "DE");
  expect_tag("es-419", "es", "", "419");
}

TEST(LocaleUtil, a_tag_takes_any_case_and_an_underscore) {
  expect_tag("DE-de", "de", "", "DE");
  expect_tag("de_CH", "de", "", "CH");
  expect_tag("SR-LATN-rs", "sr", "Latn", "RS");
}

TEST(LocaleUtil, a_script_is_not_a_region) {
  expect_tag("zh-Hans", "zh", "Hans", "");
  expect_tag("pt-Latn-BR", "pt", "Latn", "BR");
}

TEST(LocaleUtil, a_variant_and_an_extension_are_dropped) {
  expect_tag("de-CH-1996", "de", "", "CH");
  expect_tag("sl-rozaj", "sl", "", "");
  expect_tag("en-u-ca-gregory", "en", "", "");
  expect_tag("en-x-DE", "en", "", "");
}

TEST(LocaleUtil, norwegian_is_bokmal) {
  expect_tag("no", "nb", "", "");
  expect_tag("no-NO", "nb", "", "NO");
  expect_tag("nn-NO", "nn", "", "NO");
}

TEST(LocaleUtil, an_empty_tag_states_nothing) { expect_tag("", "", "", ""); }
