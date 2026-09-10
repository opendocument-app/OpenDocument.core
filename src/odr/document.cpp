#include <odr/document.hpp>

#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/util/file_util.hpp>

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

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
  std::ofstream out = internal::util::file::create(path);
  m_impl->save(out);
}

void Document::save(const std::string &path,
                    const std::string &password) const {
  if (!m_impl->is_savable(true)) {
    throw UnsupportedOperation();
  }
  std::ofstream out = internal::util::file::create(path);
  m_impl->save(out, password.c_str());
}

void Document::save(std::ostream &out) const {
  if (!m_impl->is_savable(false)) {
    throw UnsupportedOperation();
  }
  m_impl->save(out);
}

void Document::save(std::ostream &out, const std::string &password) const {
  if (!m_impl->is_savable(true)) {
    throw UnsupportedOperation();
  }
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

namespace {

/// `{"type": "number", "number": …, "text": …}`, or `"string"` with the text
/// alone, or `"empty"` for a cell stating nothing.
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
  throw std::invalid_argument("unknown cell value type " + type);
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
  if (json.value("version", 0) != 2) {
    throw std::invalid_argument("unsupported edit version");
  }

  // an operation that creates an element states a negative id for it, and a
  // later one names it by that number (`docs/design/document-editing.md`)
  std::unordered_map<std::int64_t, ElementIdentifier> minted;

  // the element @p field names, checked to be one this document holds
  const auto element_of = [&](const nlohmann::json &operation,
                              const char *field) {
    const auto address = operation.at(field).get<std::int64_t>();
    ElementIdentifier identifier{};
    if (address < 0) {
      const auto entry = minted.find(address);
      if (entry == std::end(minted)) {
        throw std::invalid_argument("element " + std::to_string(address) +
                                    " has not been created");
      }
      identifier = entry->second;
    } else {
      identifier = static_cast<ElementIdentifier>(address);
    }
    const Element element = element_by_id(identifier);
    if (!element) {
      throw std::invalid_argument("element " + std::to_string(address) +
                                  " not found");
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

  // the negative id an operation reserves, checked before anything is created
  // so that a refusal changes nothing
  const auto reserve = [&](const nlohmann::json &operation) {
    const auto address = operation.at("id").get<std::int64_t>();
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
      sheet_at(root_element(), operation.at("sheet").get<std::uint32_t>())
          .set_cell(operation.at("column").get<std::uint32_t>(),
                    operation.at("row").get<std::uint32_t>(),
                    parse_cell_value(operation.at("value")));
      continue;
    }

    if (name == "setText") {
      text_of(operation, "id")
          .set_content(operation.at("text").get<std::string>());
      continue;
    }

    if (name == "insertText") {
      const bool after = operation.contains("after");
      if (after == operation.contains("before")) {
        throw std::invalid_argument(
            "insertText names one of `after` and `before`");
      }
      const std::int64_t address = reserve(operation);
      const Text anchor = text_of(operation, after ? "after" : "before");
      const auto text = operation.at("text").get<std::string>();
      const Text created = after ? insert_text_after(anchor, text)
                                 : insert_text_before(anchor, text);
      minted.emplace(address, created.identifier());
      continue;
    }

    if (name == "removeElement") {
      remove(element_of(operation, "id"));
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
  try {
    // throwing is how a registry answers an id it does not hold
    static_cast<void>(adapter->element_type(identifier));
  } catch (const std::out_of_range &) {
    return {};
  }
  return {adapter, identifier};
}

ElementIdentifier Document::check_(const Element &element) const {
  if (element_by_id(element.identifier()) != element) {
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

Filesystem Document::as_filesystem() const {
  if (std::shared_ptr<internal::abstract::ReadableFilesystem> files =
          m_impl->as_filesystem()) {
    return Filesystem(std::move(files));
  }
  // a document that is one file has no files of its own rather than no answer
  return Filesystem(std::make_shared<internal::VirtualFilesystem>());
}

} // namespace odr
