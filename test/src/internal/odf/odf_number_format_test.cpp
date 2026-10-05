#include <odr/internal/odf/odf_number_format.hpp>

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <pugixml.hpp>

using namespace odr::internal::odf;

namespace {

/// The format code of the first data style of @p styles, the others found
/// by name.
std::optional<std::string> code_of(const std::string &styles) {
  pugi::xml_document document;
  document.load_string(
      ("<office:styles>" + styles + "</office:styles>").c_str());
  const pugi::xml_node root = document.document_element();
  return format_code(root.first_child(), [&root](const std::string_view name) {
    return root.find_child_by_attribute("style:name",
                                        std::string(name).c_str());
  });
}

} // namespace

TEST(OdfNumberFormat, a_number_style_is_its_placeholders) {
  EXPECT_EQ(code_of(R"(<number:number-style style:name="N0">)"
                    R"(<number:number number:min-integer-digits="1"/>)"
                    R"(</number:number-style>)"),
            "General");
  EXPECT_EQ(code_of(R"(<number:number-style style:name="N4">)"
                    R"(<number:number number:decimal-places="2")"
                    R"( number:min-integer-digits="1" number:grouping="true"/>)"
                    R"(</number:number-style>)"),
            "#,##0.00");
  EXPECT_EQ(
      code_of(R"(<number:number-style style:name="N">)"
              R"(<number:number number:decimal-places="3")"
              R"( number:min-decimal-places="1" number:min-integer-digits="2")"
              R"( number:display-factor="1000"/>)"
              R"(<number:text> k</number:text></number:number-style>)"),
      "00.0##,\" k\"");
  for (const std::string factor : {"100", "inf", "1e999", "0"}) {
    EXPECT_FALSE(code_of(
        "<number:number-style><number:number number:decimal-places=\"2\" "
        "number:display-factor=\"" +
        factor + "\"/></number:number-style>"));
  }
}

TEST(OdfNumberFormat, a_percent_scales_and_a_currency_is_a_literal) {
  EXPECT_EQ(
      code_of(R"(<number:percentage-style style:name="P">)"
              R"(<number:number number:decimal-places="1")"
              R"( number:min-integer-digits="1"/>)"
              R"(<number:text>%</number:text></number:percentage-style>)"),
      "0.0%");
  EXPECT_EQ(code_of(R"(<number:currency-style style:name="C">)"
                    R"(<number:number number:decimal-places="2")"
                    R"( number:min-integer-digits="1" number:grouping="true"/>)"
                    R"(<number:text> </number:text>)"
                    R"(<number:currency-symbol>€</number:currency-symbol>)"
                    R"(</number:currency-style>)"),
            "#,##0.00\" \"\"€\"");
}

/// As LibreOffice writes a red negative: the style for the negative values
/// maps the others onto the positive one.
TEST(OdfNumberFormat, a_sign_map_is_two_sections) {
  EXPECT_EQ(
      code_of(
          R"(<number:currency-style style:name="C-">)"
          R"(<number:text>-</number:text>)"
          R"(<number:number number:decimal-places="2" number:min-integer-digits="1"/>)"
          R"(<style:map style:condition="value()&gt;=0" style:apply-style-name="CP"/>)"
          R"(</number:currency-style>)"
          R"(<number:currency-style style:name="CP">)"
          R"(<number:number number:decimal-places="2" number:min-integer-digits="1"/>)"
          R"(</number:currency-style>)"),
      "0.00;\"-\"0.00");
}

TEST(OdfNumberFormat, dates_times_and_what_has_no_code) {
  EXPECT_EQ(
      code_of(
          R"(<number:date-style style:name="D">)"
          R"(<number:day number:style="long"/><number:text>.</number:text>)"
          R"(<number:month number:textual="true"/><number:text> </number:text>)"
          R"(<number:year number:style="long"/></number:date-style>)"),
      "dd\".\"mmm\" \"yyyy");
  EXPECT_EQ(
      code_of(R"(<number:time-style style:name="T")"
              R"( number:truncate-on-overflow="false">)"
              R"(<number:hours/><number:text>:</number:text>)"
              R"(<number:minutes number:style="long"/></number:time-style>)"),
      "[h]\":\"mm");
  EXPECT_EQ(code_of(R"(<number:date-style style:name="E"><number:era/>)"
                    R"(</number:date-style>)"),
            std::nullopt);
}

TEST(OdfNumberFormat, placeholder_counts_are_clamped) {
  constexpr std::array<std::pair<const char *, const char *>, 7> fields{{
      {"number:number", "number:decimal-places"},
      {"number:number", "number:min-decimal-places"},
      {"number:number", "number:min-integer-digits"},
      {"number:scientific-number", "number:min-exponent-digits"},
      {"number:fraction", "number:min-numerator-digits"},
      {"number:fraction", "number:min-denominator-digits"},
      {"number:seconds", "number:decimal-places"},
  }};
  for (const auto &[node, attribute] : fields) {
    for (const std::string count :
         {"-2147483648", "4097", "2147483647", "4294967296"}) {
      const std::string xml = std::string("<number:number-style><") + node +
                              " " + attribute + "=\"" + count +
                              "\"/></number:number-style>";
      SCOPED_TRACE(xml);
      const auto code = code_of(xml);
      ASSERT_TRUE(code);
      EXPECT_LE(code->size(), 4096 + 16);
    }
  }
  EXPECT_EQ(
      code_of(R"(<number:number-style><number:number )"
              R"(number:decimal-places="1" number:min-decimal-places="2"/>)"
              R"(</number:number-style>)"),
      "0.0");
  EXPECT_EQ(
      code_of(R"(<number:number-style><number:number )"
              R"(number:decimal-places="0" number:min-integer-digits="5000"/>)"
              R"(</number:number-style>)"),
      std::string(4096, '0'));
}
