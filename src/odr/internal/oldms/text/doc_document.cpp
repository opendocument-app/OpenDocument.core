#include <odr/internal/oldms/text/doc_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/oldms/text/doc_parser.hpp>
#include <odr/internal/util/document_util.hpp>

namespace odr::internal::oldms::text {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::legacy_word_document, DocumentType::text,
                         std::move(files)) {
  m_root_element = parse_tree(m_element_registry, *m_files);

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

ElementRegistry &Document::element_registry() { return m_element_registry; }

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

bool Document::is_editable() const noexcept { return true; }

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void Document::save(const Path &path) const {
  (void)path;
  throw UnsupportedOperation();
}

void Document::save(const Path &path, const char *password) const {
  (void)path;
  (void)password;
  throw UnsupportedOperation();
}

namespace {

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::TextRootAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry)
      : m_document(&document), m_registry(&registry) {}

  [[nodiscard]] ElementType
  element_type(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).type;
  }

  [[nodiscard]] ElementIdentifier
  element_parent(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).parent_id;
  }
  [[nodiscard]] ElementIdentifier
  element_first_child(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).first_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_last_child(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).last_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).previous_sibling_id;
  }
  [[nodiscard]] ElementIdentifier
  element_next_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).next_sibling_id;
  }

  [[nodiscard]] bool element_is_unique(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return true;
  }
  [[nodiscard]] bool element_is_self_locatable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return true;
  }
  [[nodiscard]] bool element_is_editable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return false;
  }
  [[nodiscard]]
  DocumentPath
  element_document_path(const ElementIdentifier element_id) const override {
    return util::document::extract_path(*this, element_id, null_element_id);
  }
  [[nodiscard]] ElementIdentifier
  element_navigate_path(const ElementIdentifier element_id,
                        const DocumentPath &path) const override {
    return util::document::navigate_path(*this, element_id, path);
  }

  [[nodiscard]] const TextRootAdapter *
  text_root_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::root ? this : nullptr;
  }
  [[nodiscard]] const LineBreakAdapter *
  line_break_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::line_break ? this : nullptr;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::paragraph ? this : nullptr;
  }
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::span ? this : nullptr;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::text ? this : nullptr;
  }

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
    (void)element_id;
    return {}; // TODO
  }

  [[nodiscard]] TextStyle
  span_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
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
    (void)element_id;
    return {}; // TODO
  }

private:
  // TODO remove maybe_unused
  [[maybe_unused]]
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::oldms::text
