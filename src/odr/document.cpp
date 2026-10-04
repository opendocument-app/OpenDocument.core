#include <odr/document.hpp>

#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/sheet_dependencies.hpp>
#include <odr/internal/common/sheet_recalculation.hpp>
#include <odr/internal/util/file_util.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace odr {

Document::Document(std::shared_ptr<internal::abstract::Document> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw NullPointerError("document is null");
  }
}

bool Document::is_editable() const noexcept { return m_impl->is_editable(); }

bool Document::is_savable(const bool encrypted) const noexcept {
  return m_impl->is_savable(encrypted);
}

// Every overload checks before it writes, so an unsavable format leaves no
// empty file behind and no half-written stream.
void Document::save(const std::string &path) const {
  if (!m_impl->is_savable(false)) {
    throw UnsupportedOperation();
  }
  recalculate_edits_();
  std::ofstream out = internal::util::file::create(path);
  m_impl->save(out);
}

void Document::save(const std::string &path,
                    const std::string &password) const {
  if (!m_impl->is_savable(true)) {
    throw UnsupportedOperation();
  }
  recalculate_edits_();
  std::ofstream out = internal::util::file::create(path);
  m_impl->save(out, password.c_str());
}

void Document::save(std::ostream &out) const {
  if (!m_impl->is_savable(false)) {
    throw UnsupportedOperation();
  }
  recalculate_edits_();
  m_impl->save(out);
}

void Document::save(std::ostream &out, const std::string &password) const {
  if (!m_impl->is_savable(true)) {
    throw UnsupportedOperation();
  }
  recalculate_edits_();
  m_impl->save(out, password.c_str());
}

File Document::save_to_memory() const {
  std::ostringstream out;
  save(out);
  return File::from_memory(std::move(out).str());
}

File Document::save_to_memory(const std::string &password) const {
  std::ostringstream out;
  save(out, password);
  return File::from_memory(std::move(out).str());
}

FileType Document::file_type() const noexcept { return m_impl->file_type(); }

DocumentType Document::document_type() const noexcept {
  return m_impl->document_type();
}

std::optional<std::string> Document::locale() const { return m_impl->locale(); }

namespace {

template <typename T> T integer(const nlohmann::json &value) {
  if (value.is_number_unsigned()) {
    const auto number = value.get<std::uint64_t>();
    if (std::in_range<T>(number)) {
      return static_cast<T>(number);
    }
  } else if (value.is_number_integer()) {
    const auto number = value.get<std::int64_t>();
    if (std::in_range<T>(number)) {
      return static_cast<T>(number);
    }
  }
  throw std::invalid_argument("edit integer is out of range");
}

/// `{"type": "number", "number": …, "text": …}`, `"date"` and `"time"` the
/// same with days since 1899-12-30, or `"string"` with the text alone, or
/// `"empty"` for a cell stating nothing.
CellValue parse_cell_value(const nlohmann::json &json) {
  const auto type = json.at("type").get<std::string>();
  if (type == "empty") {
    return {};
  }
  if (type == "string") {
    return CellValue(json.at("text").get<std::string>());
  }
  if (type == "number") {
    const auto number = json.at("number").get<double>();
    const auto text = json.find("text");
    return text != std::end(json) ? CellValue(number, text->get<std::string>())
                                  : CellValue(number);
  }
  if (type == "date" || type == "time") {
    CellValue result =
        CellValue(type == "date" ? ValueType::date : ValueType::time)
            .with_number(json.at("number").get<double>());
    if (const auto text = json.find("text"); text != std::end(json)) {
      result = result.with_text(text->get<std::string>());
    }
    return result;
  }
  throw std::invalid_argument("unknown cell value type " + type);
}

/// `#rrggbb`, as the page spells one.
Color parse_color(const std::string &text) {
  const auto is_hex = [](const char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
  };
  if (text.size() != 7 || text[0] != '#' ||
      !std::ranges::all_of(text.substr(1), is_hex)) {
    throw std::invalid_argument("not a color: " + text);
  }
  return Color::from_rgb(
      static_cast<std::uint32_t>(std::strtoul(text.c_str() + 1, nullptr, 16)));
}

/// A length with a fixed size, as `Measure` spells it (`14pt`).
Measure parse_font_size(const std::string &text) {
  static constexpr std::array<std::string_view, 6> units{"pt", "px", "in",
                                                         "cm", "mm", "pc"};
  const Measure size(text);
  if (!(size.magnitude() > 0) ||
      !std::ranges::contains(units, size.unit().name())) {
    throw std::invalid_argument("not a font size: " + text);
  }
  return size;
}

/// The `style` of a `setTextStyle` op: a toggle as a bool, a colour as
/// `#rrggbb`, `null` for no highlight (`docs/design/document-editing.md`).
TextStyle parse_text_style(const nlohmann::json &json) {
  TextStyle style;
  for (const auto &[key, value] : json.items()) {
    if (key == "bold") {
      style.font_weight =
          value.get<bool>() ? FontWeight::bold : FontWeight::normal;
    } else if (key == "italic") {
      style.font_style =
          value.get<bool>() ? FontStyle::italic : FontStyle::normal;
    } else if (key == "underline") {
      style.font_underline = value.get<bool>();
    } else if (key == "strikethrough") {
      style.font_line_through = value.get<bool>();
    } else if (key == "highlight") {
      style.background_color = value.is_null()
                                   ? Color(0, 0, 0, 0)
                                   : parse_color(value.get<std::string>());
    } else if (key == "color") {
      style.font_color = parse_color(value.get<std::string>());
    } else if (key == "size") {
      style.font_size = parse_font_size(value.get<std::string>());
    } else {
      throw std::invalid_argument("unknown text style property " + key);
    }
  }
  return style;
}

/// The `style` of a `setCellStyle` op: the keys of `setTextStyle` but
/// `highlight`, and `fill` and `align`.
std::pair<TableCellStyle, TextStyle>
parse_cell_style(const nlohmann::json &json) {
  TableCellStyle cell_style;
  nlohmann::json text_keys = nlohmann::json::object();
  for (const auto &[key, value] : json.items()) {
    if (key == "fill") {
      cell_style.background_color = value.is_null()
                                        ? Color(0, 0, 0, 0)
                                        : parse_color(value.get<std::string>());
    } else if (key == "align") {
      if (value.is_null()) {
        cell_style.horizontal_align = HorizontalAlign::general;
        continue;
      }
      const auto align = value.get<std::string>();
      if (align == "left") {
        cell_style.horizontal_align = HorizontalAlign::left;
      } else if (align == "center") {
        cell_style.horizontal_align = HorizontalAlign::center;
      } else if (align == "right") {
        cell_style.horizontal_align = HorizontalAlign::right;
      } else {
        throw std::invalid_argument("unknown alignment " + align);
      }
    } else if (key == "highlight") {
      throw std::invalid_argument(
          "a cell has no highlight; `fill` is its ground");
    } else {
      text_keys[key] = value;
    }
  }
  return {cell_style, parse_text_style(text_keys)};
}

/// The `style` of a `setParagraphStyle` op: `align` as `left`, `center`,
/// `right` or `justify`.
ParagraphStyle parse_paragraph_style(const nlohmann::json &json) {
  ParagraphStyle style;
  for (const auto &[key, value] : json.items()) {
    if (key != "align") {
      throw std::invalid_argument("unknown paragraph style property " + key);
    }
    const auto align = value.get<std::string>();
    if (align == "left") {
      style.text_align = TextAlign::left;
    } else if (align == "center") {
      style.text_align = TextAlign::center;
    } else if (align == "right") {
      style.text_align = TextAlign::right;
    } else if (align == "justify") {
      style.text_align = TextAlign::justify;
    } else {
      throw std::invalid_argument("unknown alignment " + align);
    }
  }
  return style;
}

/// The @p ordinal -th sheet in document order, which is how an op names one.
Sheet sheet_at(const Element root, const std::uint32_t ordinal) {
  std::uint32_t seen = 0;
  for (const Element child : root.children()) {
    if (child.type() == ElementType::sheet && seen++ == ordinal) {
      return child.as_sheet();
    }
  }
  throw std::invalid_argument("sheet " + std::to_string(ordinal) +
                              " not found");
}

} // namespace

void Document::edit(const std::string_view operations,
                    const Logger & /*logger*/) const {
  const nlohmann::json json = nlohmann::json::parse(operations);
  if (json.value("version", nlohmann::json{}) != 2) {
    throw std::invalid_argument("unsupported edit version");
  }

  if (!json.at("ops").is_array()) {
    throw std::invalid_argument("edit operations must be an array");
  }

  // an operation that creates an element states a negative id for it, and a
  // later one names it by that number (`docs/design/document-editing.md`)
  std::unordered_map<std::int64_t, ElementIdentifier> minted;

  // the element @p field names, checked to be one this document holds
  const auto element_of = [&](const nlohmann::json &operation,
                              const char *field) {
    const auto &address = operation.at(field);
    ElementIdentifier identifier{};
    if (address.is_number_integer() && address < 0) {
      const auto entry = minted.find(integer<std::int64_t>(address));
      if (entry == std::end(minted)) {
        throw std::invalid_argument("element " + address.dump() +
                                    " has not been created");
      }
      identifier = entry->second;
    } else {
      identifier = integer<ElementIdentifier>(address);
    }
    const Element element = element_by_id(identifier);
    if (!element) {
      throw std::invalid_argument("element " + address.dump() + " not found");
    }
    return element;
  };

  // the run @p field names, refusing an element that is not one
  const auto text_of = [&](const nlohmann::json &operation, const char *field) {
    const Element element = element_of(operation, field);
    const Text text = element.as_text();
    if (!text) {
      throw std::invalid_argument("element " +
                                  std::to_string(element.identifier()) +
                                  " is not a text element");
    }
    return text;
  };

  // the paragraph @p field names, refusing an element that is not one
  const auto paragraph_of = [&](const nlohmann::json &operation,
                                const char *field) {
    const Element element = element_of(operation, field);
    const Paragraph paragraph = element.as_paragraph();
    if (!paragraph) {
      throw std::invalid_argument("element " +
                                  std::to_string(element.identifier()) +
                                  " is not a paragraph");
    }
    return paragraph;
  };

  // the negative id an operation reserves, checked before anything is created
  // so that a refusal changes nothing
  const auto reserve = [&](const nlohmann::json &operation) {
    const auto address = integer<std::int64_t>(operation.at("id"));
    if (address >= 0) {
      throw std::invalid_argument("a created element needs a negative id");
    }
    if (minted.contains(address)) {
      throw std::invalid_argument("element " + std::to_string(address) +
                                  " has been created twice");
    }
    return address;
  };

  for (const nlohmann::json &operation : json.at("ops")) {
    const auto name = operation.at("op").get<std::string>();

    if (name == "setCell") {
      sheet_at(root_element(), integer<std::uint32_t>(operation.at("sheet")))
          .set_cell(integer<std::uint32_t>(operation.at("column")),
                    integer<std::uint32_t>(operation.at("row")),
                    parse_cell_value(operation.at("value")));
      continue;
    }

    if (name == "setCellStyle") {
      const auto [cell_style, text_style] =
          parse_cell_style(operation.at("style"));
      sheet_at(root_element(), integer<std::uint32_t>(operation.at("sheet")))
          .set_cell_style(integer<std::uint32_t>(operation.at("column")),
                          integer<std::uint32_t>(operation.at("row")),
                          cell_style, text_style);
      continue;
    }

    if (name == "setRowStyle") {
      const auto [cell_style, text_style] =
          parse_cell_style(operation.at("style"));
      sheet_at(root_element(), integer<std::uint32_t>(operation.at("sheet")))
          .set_row_style(integer<std::uint32_t>(operation.at("row")),
                         cell_style, text_style);
      continue;
    }

    if (name == "setColumnStyle") {
      const auto [cell_style, text_style] =
          parse_cell_style(operation.at("style"));
      sheet_at(root_element(), integer<std::uint32_t>(operation.at("sheet")))
          .set_column_style(integer<std::uint32_t>(operation.at("column")),
                            cell_style, text_style);
      continue;
    }

    if (name == "insertRows" || name == "deleteRows") {
      const Sheet sheet = sheet_at(
          root_element(), integer<std::uint32_t>(operation.at("sheet")));
      const auto row = integer<std::uint32_t>(operation.at("row"));
      const auto count = integer<std::uint32_t>(operation.at("count"));
      name == "insertRows" ? sheet.insert_rows(row, count)
                           : sheet.delete_rows(row, count);
      continue;
    }

    if (name == "insertColumns" || name == "deleteColumns") {
      const Sheet sheet = sheet_at(
          root_element(), integer<std::uint32_t>(operation.at("sheet")));
      const auto column = integer<std::uint32_t>(operation.at("column"));
      const auto count = integer<std::uint32_t>(operation.at("count"));
      name == "insertColumns" ? sheet.insert_columns(column, count)
                              : sheet.delete_columns(column, count);
      continue;
    }

    if (name == "setText") {
      text_of(operation, "id")
          .set_content(operation.at("text").get<std::string>());
      continue;
    }

    if (name == "setTextStyle") {
      text_of(operation, "id")
          .set_style(parse_text_style(operation.at("style")));
      continue;
    }

    if (name == "setParagraphStyle") {
      paragraph_of(operation, "id")
          .set_style(parse_paragraph_style(operation.at("style")));
      continue;
    }

    if (name == "insertText") {
      const auto text = operation.at("text").get<std::string>();
      const std::int64_t address = reserve(operation);

      // `parent` appends into an element rather than naming a run to sit
      // beside
      if (operation.contains("parent")) {
        if (operation.contains("after") || operation.contains("before")) {
          throw std::invalid_argument(
              "insertText names `parent` or a run to sit beside, not both");
        }
        const Element parent = element_of(operation, "parent");
        minted.emplace(address, append_text(parent, text).identifier());
        continue;
      }

      const bool after = operation.contains("after");
      if (after == operation.contains("before")) {
        throw std::invalid_argument(
            "insertText names one of `after`, `before` and `parent`");
      }
      const Text anchor = text_of(operation, after ? "after" : "before");
      const Text created = after ? insert_text_after(anchor, text)
                                 : insert_text_before(anchor, text);
      minted.emplace(address, created.identifier());
      continue;
    }

    if (name == "removeElement") {
      remove(element_of(operation, "id"));
      continue;
    }

    if (name == "splitParagraph") {
      const std::int64_t address = reserve(operation);
      const Paragraph paragraph = paragraph_of(operation, "paragraph");
      const Element after = operation.contains("after")
                                ? element_of(operation, "after")
                                : Element();
      minted.emplace(address, split_paragraph(paragraph, after).identifier());
      continue;
    }

    if (name == "mergeParagraph") {
      merge_paragraph_with_next(paragraph_of(operation, "paragraph"));
      continue;
    }

    if (name == "insertParagraph") {
      const std::int64_t address = reserve(operation);
      const Paragraph after = paragraph_of(operation, "after");
      minted.emplace(address, insert_paragraph_after(after).identifier());
      continue;
    }

    throw std::invalid_argument("unknown operation " + name);
  }
}

Element Document::root_element() const {
  return {m_impl->element_adapter(), m_impl->root_element()};
}

Element Document::element_by_id(const ElementIdentifier identifier) const {
  const internal::abstract::ElementAdapter *adapter = m_impl->element_adapter();
  if (adapter == nullptr || identifier == null_element_id) {
    return {};
  }
  try {
    // throwing is how a registry answers an id it does not hold
    static_cast<void>(adapter->element_type(identifier));
  } catch (const std::out_of_range &) {
    return {};
  }
  return {adapter, identifier};
}

ElementIdentifier Document::check_(const Element &element) const {
  if (!element || element_by_id(element.identifier()) != element) {
    throw std::invalid_argument("element is not this document's");
  }
  return element.identifier();
}

void Document::remove(const Element &element) const {
  m_impl->element_adapter()->element_remove(check_(element));
}

Text Document::insert_text_before(const Text &anchor,
                                  const std::string &text) const {
  return insert_text_(anchor, Placement::before, text);
}

Text Document::insert_text_after(const Text &anchor,
                                 const std::string &text) const {
  return insert_text_(anchor, Placement::after, text);
}

Text Document::insert_text_(const Text &anchor, const Placement where,
                            const std::string &text) const {
  const internal::abstract::ElementAdapter *adapter = m_impl->element_adapter();
  const ElementIdentifier anchor_id = check_(anchor);
  const internal::abstract::TextAdapter *runs =
      adapter->text_adapter(anchor_id);
  if (runs == nullptr) {
    throw std::invalid_argument("element " + std::to_string(anchor_id) +
                                " is not a text element");
  }
  const ElementIdentifier identifier =
      runs->text_insert(anchor_id, where, text);
  return {adapter, identifier, adapter->text_adapter(identifier)};
}

Text Document::append_text(const Element &parent,
                           const std::string &text) const {
  const internal::abstract::ElementAdapter *adapter = m_impl->element_adapter();
  const ElementIdentifier identifier =
      adapter->element_append_text(check_(parent), text);
  return {adapter, identifier, adapter->text_adapter(identifier)};
}

/// The adapter @p paragraph answers to, refusing an element that is not a
/// paragraph of this document.
const internal::abstract::ParagraphAdapter *
Document::paragraphs_(const Paragraph &paragraph,
                      ElementIdentifier &identifier) const {
  identifier = check_(paragraph);
  const internal::abstract::ParagraphAdapter *paragraphs =
      m_impl->element_adapter()->paragraph_adapter(identifier);
  if (paragraphs == nullptr) {
    throw std::invalid_argument("element " + std::to_string(identifier) +
                                " is not a paragraph");
  }
  return paragraphs;
}

Paragraph Document::split_paragraph(const Paragraph &paragraph,
                                    const Element &after) const {
  ElementIdentifier paragraph_id{};
  const internal::abstract::ParagraphAdapter *paragraphs =
      paragraphs_(paragraph, paragraph_id);
  // an element that does not exist splits before every child: Enter at the
  // start of the paragraph
  const ElementIdentifier after_id = after ? check_(after) : null_element_id;

  const internal::abstract::ElementAdapter *adapter = m_impl->element_adapter();
  const ElementIdentifier identifier =
      paragraphs->paragraph_split(paragraph_id, after_id);
  return {adapter, identifier, adapter->paragraph_adapter(identifier)};
}

void Document::merge_paragraph_with_next(const Paragraph &paragraph) const {
  ElementIdentifier paragraph_id{};
  paragraphs_(paragraph, paragraph_id)->paragraph_merge_next(paragraph_id);
}

Paragraph Document::insert_paragraph_after(const Paragraph &paragraph) const {
  ElementIdentifier paragraph_id{};
  const internal::abstract::ParagraphAdapter *paragraphs =
      paragraphs_(paragraph, paragraph_id);

  const internal::abstract::ElementAdapter *adapter = m_impl->element_adapter();
  const ElementIdentifier identifier =
      paragraphs->paragraph_insert_after(paragraph_id);
  return {adapter, identifier, adapter->paragraph_adapter(identifier)};
}

std::vector<SheetPosition>
Document::dependents(const SheetPosition &position) const {
  return dependents(std::vector<SheetPosition>{position});
}

std::vector<SheetPosition>
Document::dependents(const std::vector<SheetPosition> &positions) const {
  return m_impl->sheet_dependencies().dependents(positions);
}

std::vector<SheetPosition> Document::unresolved_formulas() const {
  return m_impl->sheet_dependencies().unresolved();
}

Recalculation Document::recalculate() const {
  internal::SheetRecalculation result = internal::recalculate(*m_impl);
  return Recalculation(std::move(result.changed), std::move(result.circular),
                       std::move(result.unevaluated));
}

void Document::recalculate_edits_() const {
  if (!internal::is_edited(*m_impl)) {
    return;
  }
  // a document a recalculation cannot read saves what its edits left
  try {
    recalculate();
  } catch (const UnsupportedOperation &) {
  }
}

Recalculation::Recalculation(std::vector<SheetPosition> changed,
                             std::vector<SheetPosition> circular,
                             std::vector<SheetPosition> unevaluated) noexcept
    : m_changed{std::move(changed)}, m_circular{std::move(circular)},
      m_unevaluated{std::move(unevaluated)} {}

const std::vector<SheetPosition> &Recalculation::changed() const noexcept {
  return m_changed;
}

const std::vector<SheetPosition> &Recalculation::circular() const noexcept {
  return m_circular;
}

const std::vector<SheetPosition> &Recalculation::unevaluated() const noexcept {
  return m_unevaluated;
}

Filesystem Document::as_filesystem() const {
  if (std::shared_ptr<internal::abstract::ReadableFilesystem> files =
          m_impl->as_filesystem()) {
    return Filesystem(std::move(files));
  }
  // a document that is one file has no files of its own rather than no answer
  return Filesystem(std::make_shared<internal::VirtualFilesystem>());
}

} // namespace odr
