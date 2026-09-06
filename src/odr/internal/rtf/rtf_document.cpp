#include <odr/internal/rtf/rtf_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/rtf/rtf_parser.hpp>
#include <odr/internal/util/document_util.hpp>

#include <istream>
#include <memory>
#include <utility>

namespace odr::internal::rtf {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const ElementRegistry &registry);
}

Document::Document(const abstract::File &file)
    : internal::Document(FileType::rich_text_format, DocumentType::text,
                         nullptr) {
  const std::unique_ptr<std::istream> in = file.stream();
  m_root_element = parse_tree(m_element_registry, *in);

  m_element_adapter = create_element_adapter(m_element_registry);
}

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

namespace {

using AdapterBase = internal::RegistryElementAdapter<
    const ElementRegistry, abstract::TextRootAdapter,
    abstract::LineBreakAdapter, abstract::ParagraphAdapter,
    abstract::TextAdapter>;

class ElementAdapter final : public AdapterBase {
public:
  explicit ElementAdapter(const ElementRegistry &registry)
      : AdapterBase(registry) {}

  [[nodiscard]] PageLayout
  text_root_page_layout(const ElementIdentifier element_id) const override {
    // `\paperwN` and the margins arrive with `PLAN.md` stage 3
    (void)element_id;
    return {};
  }
  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }

  [[nodiscard]] TextStyle
  line_break_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }
  [[nodiscard]] TextStyle
  paragraph_text_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
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
    return {};
  }

private:
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(registry);
}

} // namespace

} // namespace odr::internal::rtf
