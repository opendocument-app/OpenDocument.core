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

TEST(LocaleUtil, to_lower_takes_any_alphabet) {
  EXPECT_EQ(to_lower(U'A', "en"), U'a');
  EXPECT_EQ(to_lower(U'a', "en"), U'a');
  EXPECT_EQ(to_lower(U'Ä', "de"), U'ä');
  EXPECT_EQ(to_lower(U'Ł', "pl"), U'ł');
  EXPECT_EQ(to_lower(U'Σ', "el"), U'σ');
  EXPECT_EQ(to_lower(U'Ж', "ru"), U'ж');
  EXPECT_EQ(to_lower(U'\U00010400', "en"), U'\U00010428');
  EXPECT_EQ(to_lower(U'1', "en"), U'1');
  EXPECT_EQ(to_lower(U'ß', "de"), U'ß');
}

TEST(LocaleUtil, to_lower_keeps_the_dot_of_a_turkish_i) {
  EXPECT_EQ(to_lower(U'I', "en"), U'i');
  EXPECT_EQ(to_lower(U'I', "tr"), U'ı');
  EXPECT_EQ(to_lower(U'I', "az"), U'ı');
  EXPECT_EQ(to_lower(U'İ', "tr"), U'i');
  EXPECT_EQ(to_lower(U'İ', "en"), U'i');
}

TEST(LocaleUtil, equals_ignore_case_reads_utf8) {
  EXPECT_TRUE(equals_ignore_case("ZÄHLENWENN", "zählenwenn", "de"));
  EXPECT_TRUE(equals_ignore_case("СУММ", "сумм", "ru"));
  EXPECT_TRUE(equals_ignore_case("YANLIŞ", "yanlış", "tr"));
  EXPECT_FALSE(equals_ignore_case("YANLIŞ", "yanliş", "tr"));
  EXPECT_TRUE(equals_ignore_case("MİN", "min", "tr"));
  EXPECT_FALSE(equals_ignore_case("SUMME", "SUMMEN", "de"));
  EXPECT_FALSE(equals_ignore_case("SUMME", "SUMM", "de"));
}
