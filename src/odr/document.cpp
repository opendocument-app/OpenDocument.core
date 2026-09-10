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

  // the element @p field names, checked to be one this document holds
  const auto element_of = [&](const nlohmann::json &operation,
                              const char *field) {
    const auto identifier = operation.at(field).get<ElementIdentifier>();
    const Element element = element_by_id(identifier);
    if (!element) {
      throw std::invalid_argument("element " + std::to_string(identifier) +
                                  " not found");
    }
    return element;
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
      const Element element = element_of(operation, "id");
      const Text text = element.as_text();
      if (!text) {
        throw std::invalid_argument("element " +
                                    std::to_string(element.identifier()) +
                                    " is not a text element");
      }
      text.set_content(operation.at("text").get<std::string>());
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

Filesystem Document::as_filesystem() const {
  if (std::shared_ptr<internal::abstract::ReadableFilesystem> files =
          m_impl->as_filesystem()) {
    return Filesystem(std::move(files));
  }
  // a document that is one file has no files of its own rather than no answer
  return Filesystem(std::make_shared<internal::VirtualFilesystem>());
}

} // namespace odr
