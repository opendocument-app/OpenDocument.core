#include <odr/internal/oldms/presentation/ppt_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/util/document_util.hpp>

#include <optional>
#include <string>

namespace odr::internal::oldms::presentation {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::legacy_powerpoint_presentation,
                         DocumentType::presentation, std::move(files)) {
  m_root_element = parse_tree(m_element_registry, *m_files);

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

ElementRegistry &Document::element_registry() { return m_element_registry; }

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
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
                             public abstract::SlideAdapter,
                             public abstract::FrameAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
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
    return true;
  }
  [[nodiscard]] bool element_is_self_locatable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool element_is_editable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return false;
  }
  [[nodiscard]] DocumentPath
  element_document_path(const ElementIdentifier element_id) const override {
    return util::document::extract_path(*this, element_id, null_element_id);
  }
  [[nodiscard]] ElementIdentifier
  element_navigate_path(const ElementIdentifier element_id,
                        const DocumentPath &path) const override {
    return util::document::navigate_path(*this, element_id, path);
  }

  [[nodiscard]] const SlideAdapter *
  slide_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::slide ? this : nullptr;
  }
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::frame ? this : nullptr;
  }
  [[nodiscard]] const LineBreakAdapter *
  line_break_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::line_break ? this : nullptr;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::paragraph ? this : nullptr;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::text ? this : nullptr;
  }

  [[nodiscard]] PageLayout slide_page_layout(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // Default 4:3 slide so the renderer has non-empty page dimensions.
    return {
        .width = Measure("10in"),
        .height = Measure("7.5in"),
        .print_orientation = {},
        .margin = {},
    };
  }
  [[nodiscard]] ElementIdentifier slide_master_page(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return null_element_id;
  }
  [[nodiscard]] std::string slide_name(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] AnchorType frame_anchor_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<std::string>
  frame_x(const ElementIdentifier element_id) const override {
    return anchor_measure(element_id, [](const Anchor &a) { return a.left; });
  }
  [[nodiscard]] std::optional<std::string>
  frame_y(const ElementIdentifier element_id) const override {
    return anchor_measure(element_id, [](const Anchor &a) { return a.top; });
  }
  [[nodiscard]] std::optional<std::string>
  frame_width(const ElementIdentifier element_id) const override {
    return anchor_measure(element_id,
                          [](const Anchor &a) { return a.right - a.left; });
  }
  [[nodiscard]] std::optional<std::string>
  frame_height(const ElementIdentifier element_id) const override {
    return anchor_measure(element_id,
                          [](const Anchor &a) { return a.bottom - a.top; });
  }
  [[nodiscard]] std::optional<std::string>
  frame_z_index(const ElementIdentifier /*element_id*/) const override {
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier /*element_id*/) const override {
    return {};
  }

  [[nodiscard]] TextStyle line_break_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {}; // TODO
  }

  [[nodiscard]] ParagraphStyle paragraph_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {}; // TODO
  }
  [[nodiscard]] TextStyle paragraph_text_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // Set a font size so empty paragraphs still have height.
    TextStyle style{};
    style.font_size = Measure("11pt");
    return style;
  }

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    return m_registry->text_element_at(element_id).text;
  }
  void
  text_set_content([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::string &text) const override {
    throw UnsupportedOperation();
  }
  [[nodiscard]] TextStyle text_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // Set a font size so the text is not invisible.
    TextStyle style{};
    style.font_size = Measure("11pt");
    return style;
  }

private:
  // Converts one field of a frame's anchor (master units = 1/576 inch) to a
  // Measure string, or nullopt when the frame has no anchor.
  template <typename Selector>
  [[nodiscard]] std::optional<std::string>
  anchor_measure(const ElementIdentifier element_id,
                 const Selector &select) const {
    const std::optional<Anchor> &anchor =
        m_registry->frame_element_at(element_id).anchor;
    if (!anchor.has_value()) {
      return std::nullopt;
    }
    return Measure(select(*anchor) / 576.0, DynamicUnit("in")).to_string();
  }

  [[maybe_unused]]
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::oldms::presentation
