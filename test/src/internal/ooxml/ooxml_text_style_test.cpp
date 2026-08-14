#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <pugixml.hpp>

using namespace odr;
using namespace odr::internal::ooxml::text;

namespace {

/// `count` styles, `s0` based on `s1` and so on; only the last names a font
/// size, so reading it off `s0` proves the whole chain resolved.
std::string based_on_chain(const std::size_t count) {
  std::string result = "<w:styles>";
  for (std::size_t i = 0; i < count; ++i) {
    result += R"(<w:style w:styleId="s)" + std::to_string(i) + R"(">)";
    if (i + 1 < count) {
      result += R"(<w:basedOn w:val="s)" + std::to_string(i + 1) + R"("/>)";
    } else {
      result += R"(<w:rPr><w:sz w:val="48"/></w:rPr>)";
    }
    result += "</w:style>";
  }
  result += "</w:styles>";
  return result;
}

StyleRegistry registry_of(const std::string &xml,
                          pugi::xml_document &document) {
  EXPECT_TRUE(document.load_string(xml.c_str()));
  return StyleRegistry(document.child("w:styles"));
}

} // namespace

TEST(ooxml_text_style, based_on_chain_inherits) {
  pugi::xml_document document;
  const StyleRegistry registry = registry_of(based_on_chain(3), document);

  const Style *style = registry.style("s0");
  ASSERT_NE(nullptr, style);
  ASSERT_NE(nullptr, style->parent());
  EXPECT_EQ("s1", style->parent()->name());
  EXPECT_EQ("s2", style->parent()->parent()->name());
  EXPECT_EQ(nullptr, style->parent()->parent()->parent());

  ASSERT_TRUE(style->resolved().text_style.font_size.has_value());
  EXPECT_EQ(Measure(24, DynamicUnit("pt")),
            *style->resolved().text_style.font_size);
}

/// On a thread, whose stack is the small one an http worker gets.
TEST(ooxml_text_style, deep_based_on_chain_resolves) {
  constexpr std::size_t count = 100000;

  std::optional<Measure> font_size;
  bool last_is_root = false;

  std::thread worker([&font_size, &last_is_root] {
    pugi::xml_document document;
    const StyleRegistry registry = registry_of(based_on_chain(count), document);

    if (const Style *style = registry.style("s0"); style != nullptr) {
      font_size = style->resolved().text_style.font_size;
    }
    if (const Style *last = registry.style("s" + std::to_string(count - 1));
        last != nullptr) {
      last_is_root = last->parent() == nullptr;
    }
  });
  worker.join();

  ASSERT_TRUE(font_size.has_value());
  EXPECT_EQ(Measure(24, DynamicUnit("pt")), *font_size);
  EXPECT_TRUE(last_is_root);
}

/// A `w:basedOn` cycle resolves to styles that exist and end somewhere.
TEST(ooxml_text_style, cyclic_based_on_chain_terminates) {
  pugi::xml_document document;
  const StyleRegistry registry =
      registry_of(R"(<w:styles>)"
                  R"(<w:style w:styleId="a"><w:basedOn w:val="b"/></w:style>)"
                  R"(<w:style w:styleId="b"><w:basedOn w:val="c"/></w:style>)"
                  R"(<w:style w:styleId="c"><w:basedOn w:val="a"/></w:style>)"
                  R"(</w:styles>)",
                  document);

  for (const char *name : {"a", "b", "c"}) {
    const Style *style = registry.style(name);
    ASSERT_NE(nullptr, style) << name;
    for (std::size_t depth = 0; style != nullptr; ++depth) {
      ASSERT_LT(depth, 3u) << name;
      style = style->parent();
    }
  }
}

/// An unknown `w:basedOn` target leaves the style parentless.
TEST(ooxml_text_style, unknown_based_on_target) {
  pugi::xml_document document;
  const StyleRegistry registry = registry_of(
      R"(<w:styles><w:style w:styleId="a"><w:basedOn w:val="gone"/></w:style></w:styles>)",
      document);

  const Style *style = registry.style("a");
  ASSERT_NE(nullptr, style);
  EXPECT_EQ(nullptr, style->parent());
  EXPECT_EQ(nullptr, registry.style("gone"));
}
