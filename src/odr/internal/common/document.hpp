#pragma once

#include <odr/definitions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/document.hpp>

#include <memory>
#include <optional>
#include <string>

namespace odr::internal::zip::util {
class Archive;
} // namespace odr::internal::zip::util

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal {

/// What an engine's adapters implement. @ref Document keeps the type-specific
/// ones that match its document type.
struct EngineAdapters final {
  std::unique_ptr<abstract::ElementAdapter> element;
  const abstract::SpreadsheetAdapter *spreadsheet{nullptr};
  const abstract::PresentationAdapter *presentation{nullptr};
  const abstract::DrawingAdapter *drawing{nullptr};
};

class Document : public abstract::Document {
public:
  /// @p archive is the package @p files reads from lazily, if any.
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files,
           EncryptionState encryption_state = EncryptionState::not_encrypted,
           std::shared_ptr<zip::util::Archive> archive = nullptr);
  ~Document() override;

  /// Read-only, which every engine but odf and ooxml text is.
  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;
  void save(std::ostream &out) const override;
  void save(std::ostream &out, const char *password) const override;
  void release_source(const AbsPath &path) const final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] DocumentType document_type() const noexcept final;
  [[nodiscard]] std::optional<std::string> locale() const override;

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  as_filesystem() const noexcept final;

  [[nodiscard]] ElementIdentifier root_element() const override;

  [[nodiscard]] const abstract::ElementAdapter *
  element_adapter() const override;

  [[nodiscard]] const abstract::SpreadsheetAdapter *
  spreadsheet_adapter() const final;
  [[nodiscard]] const abstract::PresentationAdapter *
  presentation_adapter() const final;
  [[nodiscard]] const abstract::DrawingAdapter *drawing_adapter() const final;

  /// Decoded from a package that was password-encrypted. `save` has no
  /// encryption to put back, so a savable engine refuses one.
  [[nodiscard]] bool is_decrypted() const noexcept;

protected:
  FileType m_file_type{FileType::unknown};
  DocumentType m_document_type{DocumentType::unknown};
  EncryptionState m_encryption_state{EncryptionState::not_encrypted};

  std::shared_ptr<abstract::ReadableFilesystem> m_files;
  std::shared_ptr<zip::util::Archive> m_archive;

  ElementIdentifier m_root_element{null_element_id};

  void set_adapters_(EngineAdapters adapters);

private:
  std::unique_ptr<abstract::ElementAdapter> m_element_adapter;
  const abstract::SpreadsheetAdapter *m_spreadsheet_adapter{nullptr};
  const abstract::PresentationAdapter *m_presentation_adapter{nullptr};
  const abstract::DrawingAdapter *m_drawing_adapter{nullptr};
};

} // namespace odr::internal
