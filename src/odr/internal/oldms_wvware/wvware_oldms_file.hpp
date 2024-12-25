#ifndef ODR_INTERNAL_WVWARE_OLDMS_FILE_HPP
#define ODR_INTERNAL_WVWARE_OLDMS_FILE_HPP

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>

#include <memory>

#include <wv/wv.h>

namespace odr::internal::common {
class DiskFile;
class MemoryFile;
} // namespace odr::internal::common

namespace odr::internal {

class WvWareLegacyMicrosoftFile final : public abstract::DocumentFile {
public:
  explicit WvWareLegacyMicrosoftFile(std::shared_ptr<common::DiskFile> file);
  explicit WvWareLegacyMicrosoftFile(std::shared_ptr<common::MemoryFile> file);
  ~WvWareLegacyMicrosoftFile() final;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] DocumentType document_type() const final;
  [[nodiscard]] DocumentMeta document_meta() const final;

  [[nodiscard]] bool password_encrypted() const noexcept final;
  [[nodiscard]] EncryptionState encryption_state() const noexcept final;
  bool decrypt(const std::string &password) final;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final;

  [[nodiscard]] wvParseStruct &parse_struct() const;

private:
  std::shared_ptr<abstract::File> m_file;
  GsfInput *m_gsf_input{};

  EncryptionState m_encryption_state{EncryptionState::unknown};

  wvParseStruct m_ps{};
  int m_encryption_flag{};

  void open();
};

} // namespace odr::internal

#endif // ODR_INTERNAL_WVWARE_OLDMS_FILE_HPP
