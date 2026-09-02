#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/markdown/markdown_element_registry.hpp>
#include <odr/internal/markdown/markdown_style.hpp>

#include <string_view>

namespace odr::internal::markdown {

/// A markdown file as a text document: the whole tree is built up front from
/// the decoded UTF-8 text, and the file is not needed again.
class Document final : public internal::Document {
public:
  explicit Document(std::string_view text);

  [[nodiscard]] const ElementRegistry &element_registry() const;

  [[nodiscard]] const StyleRegistry &style_registry() const;

private:
  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
};

} // namespace odr::internal::markdown
