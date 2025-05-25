#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>

#include <memory>

struct _wvParseStruct;
using wvParseStruct = struct _wvParseStruct;

namespace odr::internal::common {
class DiskFile;
class MemoryFile;
} // namespace odr::internal::common

namespace odr::internal {

class WvWareLegacyMicrosoftFile final : public abstract::DocumentFile {
public:
  explicit WvWareLegacyMicrosoftFile(std::shared_ptr<common::DiskFile> file);
  explicit WvWareLegacyMicrosoftFile(std::shared_ptr<common::MemoryFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] DocumentType document_type() const final;
  [[nodiscard]] DocumentMeta document_meta() const final;

  [[nodiscard]] bool password_encrypted() const noexcept final;
  [[nodiscard]] EncryptionState encryption_state() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::DecodedFile>
  decrypt(const std::string &password) const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final;

  [[nodiscard]] wvParseStruct &parse_struct() const;

private:
  struct ParserState;

  std::shared_ptr<abstract::File> m_file;
  std::shared_ptr<ParserState> m_parser_state;

  EncryptionState m_encryption_state{EncryptionState::unknown};

  void open();
};

} // namespace odr::internal
