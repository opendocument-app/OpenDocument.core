#pragma once

#include <odr/definitions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/document.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal {

class SheetDependencies;

class Document : public abstract::Document {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files,
           EncryptionState encryption_state = EncryptionState::not_encrypted);
  ~Document() override;

  /// Read-only, which every engine but odf and ooxml text is.
  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;
  void save(std::ostream &out) const override;
  void save(std::ostream &out, const char *password) const override;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] DocumentType document_type() const noexcept final;
  [[nodiscard]] std::optional<std::string> locale() const override;

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  as_filesystem() const noexcept final;

  [[nodiscard]] ElementIdentifier root_element() const override;

  [[nodiscard]] const abstract::ElementAdapter *
  element_adapter() const override;

  [[nodiscard]] const SheetDependencies &sheet_dependencies() const final;
  [[nodiscard]] formula::Settings formula_settings() const override;
  [[nodiscard]] std::vector<formula::Name> formula_names() const override;
  /// Invalidates the dependency graph after formulas move.
  void drop_sheet_dependencies() const noexcept;

  /// Marks dependents of (@p sheet, @p position) for recalculation.
  void note_written(ElementIdentifier sheet,
                    const TablePosition &position) const;
  /// Notes an edit that moves cells: a recalculation computes every formula
  /// again.
  void note_moved() const noexcept;
  /// The writes noted since the last recalculation, by sheet.
  [[nodiscard]] const std::vector<std::pair<ElementIdentifier, TablePosition>> &
  written() const noexcept;
  [[nodiscard]] bool moved() const noexcept;
  /// Clears the edits handled by recalculation.
  void forget_edits() const noexcept;

  /// Decoded from a package that was password-encrypted. `save` has no
  /// encryption to put back, so a savable engine refuses one.
  [[nodiscard]] bool is_decrypted() const noexcept;

protected:
  FileType m_file_type{FileType::unknown};
  DocumentType m_document_type{DocumentType::unknown};
  EncryptionState m_encryption_state{EncryptionState::not_encrypted};

  std::shared_ptr<abstract::ReadableFilesystem> m_files;

  ElementIdentifier m_root_element{null_element_id};
  std::unique_ptr<abstract::ElementAdapter> m_element_adapter;

private:
  mutable std::unique_ptr<SheetDependencies> m_sheet_dependencies;
  mutable std::vector<std::pair<ElementIdentifier, TablePosition>> m_written;
  mutable bool m_moved{false};
};

} // namespace odr::internal
