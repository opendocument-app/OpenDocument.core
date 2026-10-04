#pragma once

#include <odr/definitions.hpp>
#include <odr/logger.hpp>

#include <odr/sheet_position.hpp>

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::abstract {
class Document;
class ParagraphAdapter;
} // namespace odr::internal::abstract

namespace odr {
enum class FileType;
enum class DocumentType;
class DocumentFile;
class Element;
class File;
class Filesystem;
class Paragraph;
class Text;

/// What @ref Document::recalculate did. Each list is sorted by sheet, then
/// in reading order.
class Recalculation final {
public:
  Recalculation() noexcept = default;
  Recalculation(std::vector<SheetPosition> changed,
                std::vector<SheetPosition> circular,
                std::vector<SheetPosition> unevaluated) noexcept;

  /// The formula cells whose result changed, or that had none before.
  [[nodiscard]] const std::vector<SheetPosition> &changed() const noexcept;
  /// The cells of a cycle: each reads itself, and none has a result.
  [[nodiscard]] const std::vector<SheetPosition> &circular() const noexcept;
  /// Stale formulas the evaluator cannot resolve. ODS drops their cached
  /// results; XLSX keeps them and requests recalculation on load.
  [[nodiscard]] const std::vector<SheetPosition> &unevaluated() const noexcept;

private:
  std::vector<SheetPosition> m_changed;
  std::vector<SheetPosition> m_circular;
  std::vector<SheetPosition> m_unevaluated;
};

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
  /// The language the document states for its content, as a BCP 47 tag such
  /// as `de-DE`. Nothing where it states none; only odf states one.
  [[nodiscard]] std::optional<std::string> locale() const;

  /// Applies the version-2 edit envelope in order; see
  /// `docs/design/document-editing.md` and
  /// `docs/design/spreadsheet-editing.md`.
  /// @throws std::invalid_argument on the first invalid operation; earlier
  /// operations remain applied.
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

  /// A run beside @p anchor with the same style; may create a format wrapper.
  [[nodiscard]] Text insert_text_before(const Text &anchor,
                                        const std::string &text) const;
  [[nodiscard]] Text insert_text_after(const Text &anchor,
                                       const std::string &text) const;

  /// Appends text to a supported container, creating format wrappers as needed.
  [[nodiscard]] Text append_text(const Element &parent,
                                 const std::string &text) const;

  /// Splits @p paragraph after @p after - one of its descendants, or an
  /// element that does not exist to move every child - into a new paragraph
  /// of the same style. Refuses where an element between the two is one it
  /// will not split.
  [[nodiscard]] Paragraph split_paragraph(const Paragraph &paragraph,
                                          const Element &after) const;

  /// @p paragraph takes the children of the paragraph after it, which then
  /// goes. What @ref split_paragraph undoes.
  void merge_paragraph_with_next(const Paragraph &paragraph) const;

  /// An empty paragraph after @p paragraph, of the same style.
  [[nodiscard]] Paragraph
  insert_paragraph_after(const Paragraph &paragraph) const;

  /// @}

  /// @name Formulas
  /// The graph is built the first time one of these is asked and kept.
  /// @{

  /// The cells whose formula reads @p position, directly or through another
  /// formula. Sorted by sheet and then in reading order, each named once.
  [[nodiscard]] std::vector<SheetPosition>
  dependents(const SheetPosition &position) const;
  /// The same for a whole batch of edited positions, which costs one walk
  /// rather than one per position.
  [[nodiscard]] std::vector<SheetPosition>
  dependents(const std::vector<SheetPosition> &positions) const;

  /// The cells holding a formula whose references could not all be read: it
  /// may read any position, so a caller that must be right assumes it does.
  [[nodiscard]] std::vector<SheetPosition> unresolved_formulas() const;

  /// Recomputes stale formulas: edited dependencies, missing results and
  /// untracked reads. Structural edits invalidate all formulas; save
  /// recalculates after edits.
  /// @throws UnsupportedOperation if repeated formulas exceed the
  /// recalculation limit.
  Recalculation recalculate() const;

  /// @}

  /// The files the document is packaged from; empty for a document that is
  /// one file.
  [[nodiscard]] Filesystem as_filesystem() const;

private:
  std::shared_ptr<internal::abstract::Document> m_impl;

  /// Recalculates formulas made stale by edits before saving.
  void recalculate_edits_() const;

  /// @p element 's identifier, checked to be one this document holds.
  [[nodiscard]] ElementIdentifier check_(const Element &element) const;

  [[nodiscard]] Text insert_text_(const Text &anchor, Placement where,
                                  const std::string &text) const;

  [[nodiscard]] const internal::abstract::ParagraphAdapter *
  paragraphs_(const Paragraph &paragraph, ElementIdentifier &identifier) const;

  friend DocumentFile;
};

} // namespace odr
