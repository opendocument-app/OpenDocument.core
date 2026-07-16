#include <odr/internal/oldms/presentation/ppt_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/util/document_util.hpp>

#include <memory>
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

bool Document::is_savable(const bool encrypted) const noexcept {
  (void)encrypted;
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
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter,
                             public abstract::ImageAdapter {
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
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::span ? this : nullptr;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::text ? this : nullptr;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::image ? this : nullptr;
  }

  [[nodiscard]] PageLayout slide_page_layout(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // The DocumentAtom's slide size (master units = 1/576 inch), with the
    // default 4:3 slide as fallback so the renderer has page dimensions.
    if (const auto size = m_registry->slide_size(); size.has_value()) {
      return {
          .width = Measure(size->first / 576.0, DynamicUnit("in")),
          .height = Measure(size->second / 576.0, DynamicUnit("in")),
          .print_orientation = {},
          .margin = {},
      };
    }
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
  [[nodiscard]] std::string
  slide_name(const ElementIdentifier element_id) const override {
    // Slides carry no names in the file; number them in presentation order.
    std::size_t index = 1;
    for (ElementIdentifier id =
             m_registry->element_at(element_id).previous_sibling_id;
         id != null_element_id;
         id = m_registry->element_at(id).previous_sibling_id) {
      ++index;
    }
    return "Slide " + std::to_string(index);
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
  void
  text_set_content([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::string &text) const override {
    throw UnsupportedOperation();
  }
  [[nodiscard]] TextStyle text_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // The enclosing span carries the character style.
    return {};
  }

  [[nodiscard]] bool image_is_internal(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] std::optional<odr::File>
  image_file(const ElementIdentifier element_id) const override {
    return odr::File(std::make_shared<MemoryFile>(
        m_registry->image_element_at(element_id).data));
  }
  [[nodiscard]] std::string image_href(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

private:
  /// The character style stored for a paragraph or span element.
  [[nodiscard]] TextStyle
  stored_style(const ElementIdentifier element_id) const {
    const TextStyle *style = m_registry->element_style(element_id);
    return style != nullptr ? *style : TextStyle{};
  }

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
