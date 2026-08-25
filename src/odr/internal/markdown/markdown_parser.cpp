#include <odr/internal/markdown/markdown_parser.hpp>

#include <odr/document_element.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/markdown/markdown_element_registry.hpp>
#include <odr/internal/markdown/markdown_style.hpp>
#include <odr/internal/util/string_util.hpp>

#include <md4c.h>

#include <charconv>
#include <cstdint>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odr::internal::markdown {

namespace {

/// U+FFFD, the replacement CommonMark prescribes for a NULL character.
constexpr std::string_view replacement_character = "�";

/// A numeric character reference, or the five predefined XML entities plus
/// `&nbsp;`. md4c matches anything entity-shaped and exports no lookup table,
/// so everything else stays literal.
std::string resolve_entity(const std::string_view entity) {
  if (entity.size() < 3 || entity.front() != '&' || entity.back() != ';') {
    return std::string(entity);
  }
  const std::string_view name = entity.substr(1, entity.size() - 2);

  if (name.front() == '#') {
    const bool hexadecimal =
        name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
    const std::string_view digits = name.substr(hexadecimal ? 2 : 1);
    std::uint32_t code_point{0};
    const auto [end, error] =
        std::from_chars(digits.data(), digits.data() + digits.size(),
                        code_point, hexadecimal ? 16 : 10);
    if (error != std::errc() || end != digits.data() + digits.size()) {
      return std::string(entity);
    }
    // an unpaired surrogate or an out-of-range value is not a character
    if (code_point == 0 || code_point > 0x10FFFF ||
        (code_point >= 0xD800 && code_point <= 0xDFFF)) {
      return std::string(replacement_character);
    }
    std::string result;
    util::string::append_c32(static_cast<char32_t>(code_point), result);
    return result;
  }

  if (name == "amp") {
    return "&";
  }
  if (name == "lt") {
    return "<";
  }
  if (name == "gt") {
    return ">";
  }
  if (name == "quot") {
    return "\"";
  }
  if (name == "apos") {
    return "'";
  }
  if (name == "nbsp") {
    return " ";
  }
  return std::string(entity);
}

/// The text of an `MD_ATTRIBUTE` — a link href or an image source — with its
/// entity substrings resolved.
std::string attribute_to_string(const MD_ATTRIBUTE &attribute) {
  std::string result;
  if (attribute.text == nullptr) {
    return result;
  }
  for (std::size_t i = 0; attribute.substr_offsets[i] < attribute.size; ++i) {
    const MD_OFFSET begin = attribute.substr_offsets[i];
    const MD_OFFSET end = attribute.substr_offsets[i + 1];
    const std::string_view part(attribute.text + begin, end - begin);

    switch (attribute.substr_types[i]) {
    case MD_TEXT_ENTITY:
      result.append(resolve_entity(part));
      break;
    case MD_TEXT_NULLCHAR:
      result.append(replacement_character);
      break;
    default:
      result.append(part);
      break;
    }
  }
  return result;
}

std::optional<HorizontalAlign> horizontal_align(const MD_ALIGN align) {
  switch (align) {
  case MD_ALIGN_LEFT:
    return HorizontalAlign::left;
  case MD_ALIGN_CENTER:
    return HorizontalAlign::center;
  case MD_ALIGN_RIGHT:
    return HorizontalAlign::right;
  case MD_ALIGN_DEFAULT:
  default:
    return {};
  }
}

/// md4c's callbacks against one element stack: a block or span that has an
/// element pushes it, and its `leave` pops it again.
class Parser final {
public:
  Parser(ElementRegistry &registry, StyleRegistry &style_registry)
      : m_registry{&registry}, m_style_registry{&style_registry} {}

  [[nodiscard]] ElementIdentifier root() const noexcept { return m_root; }

  void enter_block(const MD_BLOCKTYPE type, const void *detail) {
    close_implicit_paragraph_();
    const std::size_t depth = m_stack.size();

    switch (type) {
    case MD_BLOCK_DOC: {
      const auto &[element_id, element] =
          m_registry->create_element(ElementType::root);
      m_root = element_id;
      push_(element_id);
    } break;
    case MD_BLOCK_QUOTE: {
      const auto &[element_id, element] =
          m_registry->create_element(ElementType::group);
      push_(element_id);
      ++m_quote_depth;
    } break;
    case MD_BLOCK_UL: {
      auto [element_id, element, list] = m_registry->create_list_element();
      list.type = ListType::unordered;
      push_(element_id);
      m_lists.push_back({});
    } break;
    case MD_BLOCK_OL: {
      const auto *ol = static_cast<const MD_BLOCK_OL_DETAIL *>(detail);
      auto [element_id, element, list] = m_registry->create_list_element();
      list.type = ListType::ordered;
      push_(element_id);
      m_lists.push_back({ListType::ordered, ol->start,
                         static_cast<char>(ol->mark_delimiter)});
    } break;
    case MD_BLOCK_LI: {
      const auto *li = static_cast<const MD_BLOCK_LI_DETAIL *>(detail);
      if (m_lists.empty()) {
        throw std::runtime_error("markdown: list item outside a list");
      }
      ListState &list = m_lists.back();

      auto [element_id, element, item] = m_registry->create_list_item_element();
      if (list.type == ListType::ordered) {
        // the ordinal is the element api's own, independent of the marker
        item.number = list.next_number;
        item.marker = std::to_string(list.next_number) + list.delimiter;
        ++list.next_number;
      } else {
        item.marker = "•";
      }
      if (li->is_task != 0) {
        // The model has no checkbox, so the box replaces the item's marker.
        item.marker = li->task_mark == 'x' || li->task_mark == 'X' ? "☑" : "☐";
      }
      push_(element_id);
    } break;
    case MD_BLOCK_HR:
      break; // nothing in the model
    case MD_BLOCK_H: {
      const auto *heading = static_cast<const MD_BLOCK_H_DETAIL *>(detail);
      push_paragraph_(m_style_registry->heading_style(heading->level));
      // The renderer takes only font family and size from a paragraph's text
      // style, so the weight has to ride on a span — and only the weight: the
      // heading's `em` size is already the span's font size to be relative to.
      push_span_(m_style_registry->strong_style());
    } break;
    case MD_BLOCK_CODE: {
      const auto &[element_id, element] =
          m_registry->create_element(ElementType::group);
      push_(element_id);
      m_in_code_block = true;
      m_code.clear();
    } break;
    case MD_BLOCK_HTML:
      m_in_html_block = true;
      break; // dropped, see `AGENTS.md`
    case MD_BLOCK_P:
      push_paragraph_(0);
      break;
    case MD_BLOCK_TABLE: {
      const auto *table_detail =
          static_cast<const MD_BLOCK_TABLE_DETAIL *>(detail);
      auto [element_id, element, table] = m_registry->create_table_element();
      table.dimensions = TableDimensions(table_detail->head_row_count +
                                             table_detail->body_row_count,
                                         table_detail->col_count);
      push_(element_id);

      for (unsigned i = 0; i < table_detail->col_count; ++i) {
        const auto &[column_id, column] =
            m_registry->create_element(ElementType::table_column);
        m_registry->append_column(element_id, column_id);
      }
    } break;
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
      break; // the rows hang off the table itself
    case MD_BLOCK_TR: {
      const auto &[element_id, element] =
          m_registry->create_element(ElementType::table_row);
      push_(element_id);
    } break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
      const auto *cell_detail = static_cast<const MD_BLOCK_TD_DETAIL *>(detail);
      auto [element_id, element, cell] =
          m_registry->create_table_cell_element();
      cell.horizontal_align = horizontal_align(cell_detail->align);
      push_(element_id);
      // A cell holds blocks; md4c hands its content over as inline text.
      push_paragraph_(0);
      if (type == MD_BLOCK_TH) {
        push_span_(m_style_registry->strong_style());
      }
    } break;
    }

    m_block_pushes.push_back(m_stack.size() - depth);
  }

  void leave_block(const MD_BLOCKTYPE type) {
    close_implicit_paragraph_();

    switch (type) {
    case MD_BLOCK_QUOTE:
      --m_quote_depth;
      break;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
      m_lists.pop_back();
      break;
    case MD_BLOCK_CODE:
      m_in_code_block = false;
      flush_code_();
      break;
    case MD_BLOCK_HTML:
      m_in_html_block = false;
      break;
    default:
      break;
    }

    if (m_block_pushes.empty()) {
      throw std::runtime_error("markdown: unbalanced md4c callbacks");
    }
    for (std::size_t i = 0; i < m_block_pushes.back(); ++i) {
      pop_();
    }
    m_block_pushes.pop_back();
  }

  void enter_span(const MD_SPANTYPE type, const void *detail) {
    switch (type) {
    case MD_SPAN_EM:
      push_span_(m_style_registry->emphasis_style());
      break;
    case MD_SPAN_STRONG:
      push_span_(m_style_registry->strong_style());
      break;
    case MD_SPAN_DEL:
      push_span_(m_style_registry->strikethrough_style());
      break;
    case MD_SPAN_CODE:
      push_span_(m_style_registry->monospace_style());
      break;
    case MD_SPAN_A: {
      const auto *link_detail = static_cast<const MD_SPAN_A_DETAIL *>(detail);
      open_implicit_paragraph_();
      auto [element_id, element, link] = m_registry->create_link_element();
      link.href = attribute_to_string(link_detail->href);
      push_(element_id);
    } break;
    default:
      break; // an image's alt text passes through as text, see `AGENTS.md`
    }
  }

  void leave_span(const MD_SPANTYPE type) {
    switch (type) {
    case MD_SPAN_EM:
    case MD_SPAN_STRONG:
    case MD_SPAN_DEL:
    case MD_SPAN_CODE:
    case MD_SPAN_A:
      pop_();
      break;
    default:
      break;
    }
  }

  void text(const MD_TEXTTYPE type, const std::string_view text) {
    switch (type) {
    case MD_TEXT_NULLCHAR:
      // md4c reports a NUL out of a verbatim block too, so it goes where that
      // block's own text goes rather than straight into the tree
      if (m_in_code_block) {
        m_code.append(replacement_character);
      } else if (!m_in_html_block) {
        append_text_(replacement_character);
      }
      break;
    case MD_TEXT_BR: {
      open_implicit_paragraph_();
      const auto &[element_id, element] =
          m_registry->create_element(ElementType::line_break);
      m_registry->append_child(current_(), element_id);
      m_open_text = null_element_id;
    } break;
    case MD_TEXT_SOFTBR:
      append_text_(" ");
      break;
    case MD_TEXT_ENTITY:
      append_text_(resolve_entity(text));
      break;
    case MD_TEXT_CODE:
      if (m_in_code_block) {
        // md4c reports a NUL in a verbatim block as `MD_TEXT_NULLCHAR` and then
        // re-sends the byte itself at the head of the next chunk
        for (const char character : text) {
          if (character != '\0') {
            m_code.push_back(character);
          }
        }
      } else {
        append_text_(text);
      }
      break;
    case MD_TEXT_HTML:
      break; // dropped, see `AGENTS.md`
    default:
      append_text_(text);
      break;
    }
  }

  /// A callback cannot throw through md4c's C frames, so the exception is
  /// parked here and rethrown once `md_parse` has returned.
  void fail(const std::exception_ptr &error) noexcept { m_error = error; }
  void rethrow() const {
    if (m_error) {
      std::rethrow_exception(m_error);
    }
  }

private:
  struct ListState final {
    ListType type{ListType::unordered};
    std::uint32_t next_number{1};
    char delimiter{'.'};
  };

  ElementRegistry *m_registry{nullptr};
  StyleRegistry *m_style_registry{nullptr};

  ElementIdentifier m_root{null_element_id};
  std::vector<ElementIdentifier> m_stack;
  /// The text element further text is appended to, while one is open.
  ElementIdentifier m_open_text{null_element_id};

  std::vector<ListState> m_lists;
  /// How many elements each open block pushed, so its `leave` pops as many.
  std::vector<std::size_t> m_block_pushes;
  /// A tight list item holds its text with no `MD_BLOCK_P` around it, but the
  /// renderer writes the marker into the item's first paragraph — so open one.
  ElementIdentifier m_implicit_paragraph{null_element_id};

  std::uint32_t m_quote_depth{0};
  /// A code block becomes one paragraph per line, so its text is buffered.
  bool m_in_code_block{false};
  std::string m_code;
  /// So the NUL bytes md4c reports out of a dropped html block go with it.
  bool m_in_html_block{false};

  std::exception_ptr m_error;

  /// md4c bounds nesting nowhere and `html::translate_element` recurses. Same
  /// order as `rtf::State::max_depth`.
  static constexpr std::size_t max_depth = 1024;

  [[nodiscard]] ElementIdentifier current_() const {
    if (m_stack.empty()) {
      throw std::runtime_error("markdown: content outside the document block");
    }
    return m_stack.back();
  }

  void push_(const ElementIdentifier element_id) {
    if (m_stack.size() >= max_depth) {
      throw std::runtime_error("markdown: block nesting too deep");
    }
    if (!m_stack.empty()) {
      m_registry->append_child(m_stack.back(), element_id);
    }
    m_stack.push_back(element_id);
    m_open_text = null_element_id;
  }

  void pop_() {
    if (m_stack.empty()) {
      throw std::runtime_error("markdown: unbalanced md4c callbacks");
    }
    m_stack.pop_back();
    m_open_text = null_element_id;
  }

  void push_paragraph_(const std::uint32_t text_style_index) {
    const auto &[element_id, element] =
        m_registry->create_element(ElementType::paragraph);
    if (text_style_index != 0) {
      m_registry->set_element_text_style_index(element_id, text_style_index);
    }
    if (const std::uint32_t paragraph_style_index =
            m_style_registry->quote_style(m_quote_depth);
        paragraph_style_index != 0) {
      m_registry->set_element_paragraph_style_index(element_id,
                                                    paragraph_style_index);
    }
    push_(element_id);
  }

  void push_span_(const std::uint32_t text_style_index) {
    open_implicit_paragraph_();
    const auto &[element_id, element] =
        m_registry->create_element(ElementType::span);
    m_registry->set_element_text_style_index(element_id, text_style_index);
    push_(element_id);
  }

  void open_implicit_paragraph_() {
    if (m_registry->element_at(current_()).type != ElementType::list_item) {
      return;
    }
    push_paragraph_(0);
    m_implicit_paragraph = m_stack.back();
  }

  void close_implicit_paragraph_() {
    if (m_implicit_paragraph == null_element_id || m_stack.empty() ||
        m_stack.back() != m_implicit_paragraph) {
      return;
    }
    pop_();
    m_implicit_paragraph = null_element_id;
  }

  void append_text_(const std::string_view text) {
    if (text.empty()) {
      return;
    }
    open_implicit_paragraph_();
    if (m_open_text == null_element_id) {
      auto [element_id, element, payload] = m_registry->create_text_element();
      m_registry->append_child(current_(), element_id);
      m_open_text = element_id;
      payload.text.append(text);
      return;
    }
    m_registry->text_element_at(m_open_text).text.append(text);
  }

  void flush_code_() {
    // md4c terminates every code line with '\n', the last one included
    if (m_code.ends_with('\n')) {
      m_code.pop_back();
    }

    util::string::split(m_code, "\n", [this](const std::string &line) {
      push_paragraph_(m_style_registry->monospace_style());
      append_text_(line);
      pop_();
    });

    m_code.clear();
  }
};

/// Runs one callback body, parking anything it throws: an exception must not
/// unwind through md4c's C frames.
template <typename Callback> int invoke(void *userdata, Callback &&callback) {
  auto *parser = static_cast<Parser *>(userdata);
  try {
    callback(*parser);
  } catch (...) {
    parser->fail(std::current_exception());
    return 1;
  }
  return 0;
}

} // namespace
} // namespace odr::internal::markdown

namespace odr::internal {

ElementIdentifier markdown::parse_tree(ElementRegistry &registry,
                                       StyleRegistry &style_registry,
                                       const std::string_view text) {
  Parser parser(registry, style_registry);

  MD_PARSER md_parser{};
  md_parser.flags = MD_DIALECT_GITHUB | MD_FLAG_COLLAPSEWHITESPACE;
  md_parser.enter_block = [](const MD_BLOCKTYPE type, void *detail,
                             void *userdata) {
    return invoke(userdata,
                  [&](Parser &parser) { parser.enter_block(type, detail); });
  };
  md_parser.leave_block = [](const MD_BLOCKTYPE type, void *, void *userdata) {
    return invoke(userdata, [&](Parser &parser) { parser.leave_block(type); });
  };
  md_parser.enter_span = [](const MD_SPANTYPE type, void *detail,
                            void *userdata) {
    return invoke(userdata,
                  [&](Parser &parser) { parser.enter_span(type, detail); });
  };
  md_parser.leave_span = [](const MD_SPANTYPE type, void *, void *userdata) {
    return invoke(userdata, [&](Parser &parser) { parser.leave_span(type); });
  };
  md_parser.text = [](const MD_TEXTTYPE type, const MD_CHAR *text,
                      const MD_SIZE size, void *userdata) {
    return invoke(userdata, [&](Parser &parser) {
      parser.text(type, std::string_view(text, size));
    });
  };

  // `md_parse` dereferences the pointer even for an empty document
  const int result =
      md_parse(text.empty() ? "" : text.data(),
               static_cast<MD_SIZE>(text.size()), &md_parser, &parser);
  parser.rethrow();
  if (result != 0) {
    throw std::runtime_error("markdown: md4c failed to parse the document");
  }

  return parser.root();
}

} // namespace odr::internal
