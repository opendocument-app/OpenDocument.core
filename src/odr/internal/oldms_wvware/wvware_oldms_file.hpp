#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>

struct _wvParseStruct;
using wvParseStruct = struct _wvParseStruct;

namespace odr::internal {
class DiskFile;
class MemoryFile;
} // namespace odr::internal

namespace odr::internal {

class WvWareLegacyMicrosoftFile final : public abstract::DocumentFile {
public:
  explicit WvWareLegacyMicrosoftFile(std::shared_ptr<DiskFile> file);
  explicit WvWareLegacyMicrosoftFile(std::shared_ptr<MemoryFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] DocumentType document_type() const override;
  [[nodiscard]] DocumentMeta document_meta() const override;

  [[nodiscard]] bool password_encrypted() const noexcept override;
  [[nodiscard]] EncryptionState encryption_state() const noexcept override;
  [[nodiscard]] std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const override;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

  [[nodiscard]] wvParseStruct &parse_struct() const;

private:
  struct ParserState;

  std::shared_ptr<abstract::File> m_file;
  std::shared_ptr<ParserState> m_parser_state;

  FileMeta m_file_meta;
  EncryptionState m_encryption_state{EncryptionState::unknown};

  void open();
};

} // namespace odr::internal
