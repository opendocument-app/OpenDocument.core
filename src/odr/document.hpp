#pragma once

#include <odr/definitions.hpp>
#include <odr/logger.hpp>

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr {
enum class FileType;
enum class DocumentType;
class DocumentFile;
class Element;
class File;
class Filesystem;
class Text;

/// Represents a document.
class Document final {
public:
  explicit Document(std::shared_ptr<internal::abstract::Document>);

  [[nodiscard]] bool is_editable() const noexcept;
  /// Savable, @p encrypted to ask for an encrypted save. False for a document
  /// decrypted from a password-protected package: saving one can only write it
  /// out in the clear.
  [[nodiscard]] bool is_savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  void save(std::ostream &out) const;
  void save(std::ostream &out, const std::string &password) const;

  /// The saved document as a file in memory.
  [[nodiscard]] File save_to_memory() const;
  [[nodiscard]] File save_to_memory(const std::string &password) const;

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] DocumentType document_type() const noexcept;

  /// @brief Applies @p operations to the document, in order.
  ///
  /// The wire format our browser-side editor produces:
  /// `{"version": 2, "ops": [{"op": "setCell", "sheet": 0, "column": 1,
  /// "row": 2, "value": {"type": "number", "number": 12.5, "text": "12.5"}}]}`.
  /// A value is typed `number`, `string` or `empty`; `setText` names a text
  /// element by the `id` the render wrote into the page instead
  /// (`docs/design/document-editing.md`). Editing a single element in process
  /// is @ref Text::set_content and needs none of this.
  /// @throws std::invalid_argument on the first operation it cannot apply,
  ///         leaving the ones before it applied - a host replays onto a fresh
  ///         decode.
  void edit(std::string_view operations,
            const Logger &logger = Logger::null()) const;

  [[nodiscard]] Element root_element() const;

  /// The element @ref Element::identifier handed out, or one that does not
  /// exist where this document holds no such id.
  [[nodiscard]] Element element_by_id(ElementIdentifier identifier) const;

  /// @name Structural edits
  /// Each throws `UnsupportedOperation` where the engine cannot write, and
  /// `std::invalid_argument` for an element of another document.
  /// @{

  /// Removes @p element and its subtree; its identifier stays taken.
  void remove(const Element &element) const;

  /// A run beside @p anchor, in the same parent, so it takes the same style.
  [[nodiscard]] Text insert_text_before(const Text &anchor,
                                        const std::string &text) const;
  [[nodiscard]] Text insert_text_after(const Text &anchor,
                                       const std::string &text) const;

  /// @}

  /// The files the document is packaged from; empty for a document that is
  /// one file.
  [[nodiscard]] Filesystem as_filesystem() const;

private:
  std::shared_ptr<internal::abstract::Document> m_impl;

  /// @p element 's identifier, checked to be one this document holds.
  [[nodiscard]] ElementIdentifier check_(const Element &element) const;

  [[nodiscard]] Text insert_text_(const Text &anchor, Placement where,
                                  const std::string &text) const;

  friend DocumentFile;
};

} // namespace odr
