#include <odr/internal/oldms/oldms_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/oldms_crypto.hpp>
#include <odr/internal/oldms/presentation/ppt_document.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/oldms/spreadsheet/xls_crypto.hpp>
#include <odr/internal/oldms/spreadsheet/xls_document.hpp>
#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>
#include <odr/internal/oldms/text/doc_crypto.hpp>
#include <odr/internal/oldms/text/doc_document.hpp>
#include <odr/internal/oldms/text/doc_parser.hpp>

#include <array>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace odr::internal::oldms {

namespace {
/// Probes the cleartext encryption marker; an unreadable marker leaves it
/// unknown.
std::optional<bool>
parse_password_encrypted(const FileType type,
                         const abstract::ReadableFilesystem &files) {
  switch (type) {
  case FileType::legacy_word_document:
    return text::password_encrypted(files);
  case FileType::legacy_powerpoint_presentation:
    return presentation::password_encrypted(files);
  case FileType::legacy_excel_worksheets:
    return spreadsheet::password_encrypted(files);
  default:
    return {};
  }
}

/// The original compound file, with the decrypted streams in place of their
/// ciphertext.
class DecryptedFilesystem final : public abstract::ReadableFilesystem {
public:
  DecryptedFilesystem(std::shared_ptr<abstract::ReadableFilesystem> files,
                      crypto::DecryptedStreams streams)
      : m_files{std::move(files)} {
    for (auto &[path, data] : streams) {
      m_streams.emplace(path, std::make_shared<MemoryFile>(std::move(data)));
    }
  }

  [[nodiscard]] bool exists(const AbsPath &path) const override {
    return m_files->exists(path);
  }
  [[nodiscard]] bool is_file(const AbsPath &path) const override {
    return m_files->is_file(path);
  }
  [[nodiscard]] bool is_directory(const AbsPath &path) const override {
    return m_files->is_directory(path);
  }

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const AbsPath &path) const override {
    return m_files->file_walker(path);
  }

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const AbsPath &path) const override {
    if (const auto it = m_streams.find(path); it != m_streams.end()) {
      return it->second;
    }
    return m_files->open(path);
  }

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_files;
  std::map<AbsPath, std::shared_ptr<abstract::File>> m_streams;
};

/// The decrypted streams, or nothing if `password` is wrong.
std::optional<crypto::DecryptedStreams>
decrypt_streams(const FileType type, const abstract::ReadableFilesystem &files,
                const std::string_view password) {
  switch (type) {
  case FileType::legacy_word_document:
    return text::decrypt(files, password);
  case FileType::legacy_excel_worksheets:
    return spreadsheet::decrypt(files, password);
  default:
    throw UnsupportedOperation(
        "odrcore does not support decryption of this legacy Microsoft file");
  }
}

/// The password an application writes when the user set none, for example
/// for a workbook whose structure only is protected. A file that opens with
/// it opens without a prompt, as in LibreOffice.
std::optional<std::string_view> default_password(const FileType type) {
  switch (type) {
  case FileType::legacy_excel_worksheets:
    // [MS-XLS] 2.2.10 Encryption (Password to Open).
    return "VelvetSweatshop";
  default:
    return std::nullopt;
  }
}

FileMeta parse_meta(const abstract::ReadableFilesystem &files) {
  // Each format MUST have its stream ([MS-DOC], [MS-PPT], [MS-XLS]).
  static constexpr std::array types{
      std::pair{"/WordDocument", FileType::legacy_word_document},
      std::pair{"/PowerPoint Document",
                FileType::legacy_powerpoint_presentation},
      std::pair{"/Workbook", FileType::legacy_excel_worksheets},
  };

  FileMeta result;

  for (const auto &[path, type] : types) {
    if (files.is_file(AbsPath(path))) {
      result.type = type;
      result.mimetype = mimetype_by_file_type(type);
      result.document_type = document_type_by_file_type(type);
      break;
    }
  }

  if (result.type == FileType::unknown) {
    throw UnknownFileType();
  }

  return result;
}
} // namespace

LegacyMicrosoftFile::LegacyMicrosoftFile(
    std::shared_ptr<abstract::ReadableFilesystem> files,
    std::shared_ptr<abstract::File> source)
    : m_files{std::move(files)}, m_source{std::move(source)} {
  m_file_meta = parse_meta(*m_files);

  // An unreadable encryption marker leaves the state unknown.
  const std::optional<bool> encrypted =
      parse_password_encrypted(m_file_meta.type, *m_files);
  m_file_meta.password_encrypted = encrypted.value_or(false);
  m_encryption_state = !encrypted.has_value() ? EncryptionState::unknown
                       : *encrypted           ? EncryptionState::encrypted
                                              : EncryptionState::not_encrypted;

  if (m_encryption_state != EncryptionState::encrypted) {
    return;
  }
  const std::optional<std::string_view> password =
      default_password(m_file_meta.type);
  if (!password.has_value()) {
    return;
  }
  std::optional<crypto::DecryptedStreams> streams;
  try {
    streams = decrypt_streams(m_file_meta.type, *m_files, *password);
  } catch (const std::exception &) {
    // an encryption that cannot be read still asks for a password
    return;
  }
  if (streams.has_value()) {
    m_files = std::make_shared<DecryptedFilesystem>(std::move(m_files),
                                                    std::move(*streams));
    m_file_meta.password_encrypted = false;
    m_encryption_state = EncryptionState::not_encrypted;
  }
}

std::shared_ptr<abstract::File> LegacyMicrosoftFile::file() const noexcept {
  return m_source;
}

FileType LegacyMicrosoftFile::file_type() const noexcept {
  return m_file_meta.type;
}

std::string_view LegacyMicrosoftFile::mimetype() const noexcept {
  return m_file_meta.mimetype;
}

FileMeta LegacyMicrosoftFile::file_meta() const { return m_file_meta; }

DocumentType LegacyMicrosoftFile::document_type() const {
  return m_file_meta.document_type;
}

bool LegacyMicrosoftFile::password_encrypted() const noexcept {
  return m_file_meta.password_encrypted;
}

EncryptionState LegacyMicrosoftFile::encryption_state() const noexcept {
  return m_encryption_state;
}

std::shared_ptr<abstract::DecodedFile>
LegacyMicrosoftFile::decrypt(const std::string &password) const {
  if (m_encryption_state != EncryptionState::encrypted) {
    throw NotEncryptedError();
  }

  std::optional<crypto::DecryptedStreams> streams =
      decrypt_streams(m_file_meta.type, *m_files, password);
  if (!streams.has_value()) {
    throw WrongPasswordError();
  }

  auto decrypted = std::make_shared<LegacyMicrosoftFile>(*this);
  decrypted->m_files =
      std::make_shared<DecryptedFilesystem>(m_files, std::move(*streams));
  decrypted->m_encryption_state = EncryptionState::decrypted;
  return decrypted;
}

bool LegacyMicrosoftFile::is_decodable() const noexcept {
  return m_encryption_state != EncryptionState::encrypted;
}

std::shared_ptr<abstract::Document> LegacyMicrosoftFile::document() const {
  // The parser would read the ciphertext as structure, and the caller would see
  // a parse error and not a password prompt.
  if (m_encryption_state == EncryptionState::encrypted) {
    throw FileEncryptedError();
  }

  switch (file_type()) {
  case FileType::legacy_word_document:
    return std::make_shared<text::Document>(m_files);
  case FileType::legacy_powerpoint_presentation:
    return std::make_shared<presentation::Document>(m_files);
  case FileType::legacy_excel_worksheets:
    return std::make_shared<spreadsheet::Document>(m_files);
  default:
    throw UnsupportedFileType(file_type());
  }
}

} // namespace odr::internal::oldms
