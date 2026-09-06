#include <odr/internal/oldms/text/doc_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/oldms/text/doc_parser.hpp>
#include <odr/internal/util/document_util.hpp>

namespace odr::internal::oldms::text {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry,
                       const StyleRegistry &style_registry);
}

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::legacy_word_document, DocumentType::text,
                         std::move(files)) {
  m_root_element = parse_tree(m_element_registry, m_style_registry, *m_files);

  m_element_adapter =
      create_element_adapter(*this, m_element_registry, m_style_registry);
}

ElementRegistry &Document::element_registry() { return m_element_registry; }

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

const StyleRegistry &Document::style_registry() const {
  return m_style_registry;
}

namespace {

using AdapterBase = internal::RegistryElementAdapter<
    ElementRegistry, abstract::TextRootAdapter, abstract::LineBreakAdapter,
    abstract::ParagraphAdapter, abstract::SpanAdapter, abstract::TextAdapter>;

class ElementAdapter final : public AdapterBase {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry,
                 const StyleRegistry &style_registry)
      : AdapterBase(registry), m_document(&document),
        m_style_registry(&style_registry) {}

  [[nodiscard]] PageLayout text_root_page_layout(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }
  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }

  [[nodiscard]] TextStyle
  line_break_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TextStyle
  paragraph_text_style(const ElementIdentifier element_id) const override {
    return stored_style(element_id);
  }

  [[nodiscard]] TextStyle
  span_style(const ElementIdentifier element_id) const override {
    return stored_style(element_id);
  }

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    return m_registry->text_element_at(element_id).text;
  }
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    (void)element_id;
    (void)text;
    throw UnsupportedOperation();
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    // The enclosing span carries the character style.
    (void)element_id;
    return {};
  }

private:
  // TODO remove maybe_unused
  [[maybe_unused]]
  const Document *m_document{nullptr};
  const StyleRegistry *m_style_registry{nullptr};

  /// The character style stored for a paragraph or span element.
  [[nodiscard]] TextStyle
  stored_style(const ElementIdentifier element_id) const {
    return m_style_registry->text_style(
        m_registry->element_style_index(element_id));
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry,
                       const StyleRegistry &style_registry) {
  return std::make_unique<ElementAdapter>(document, registry, style_registry);
}

} // namespace

} // namespace odr::internal::oldms::text
