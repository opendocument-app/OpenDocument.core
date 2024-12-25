#include <odr/internal/oldms_wvware/wvware_oldms_file.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>

#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-input-stdio.h>
#include <wv/wv.h>

namespace odr::internal {

WvWareLegacyMicrosoftFile::WvWareLegacyMicrosoftFile(
    std::shared_ptr<common::DiskFile> file)
    : m_file{std::move(file)} {
  GError *error = nullptr;

  m_gsf_input =
      gsf_input_stdio_new(m_file->disk_path()->string().c_str(), &error);

  if (m_gsf_input == nullptr) {
    throw std::runtime_error("gsf_input_stdio_new failed");
  }

  open();
}

WvWareLegacyMicrosoftFile::WvWareLegacyMicrosoftFile(
    std::shared_ptr<common::MemoryFile> file)
    : m_file{std::move(file)} {
  m_gsf_input = gsf_input_memory_new(
      reinterpret_cast<const guint8 *>(m_file->memory_data()),
      static_cast<gsf_off_t>(m_file->size()), false);

  open();
}

WvWareLegacyMicrosoftFile::~WvWareLegacyMicrosoftFile() { wvOLEFree(&m_ps); }

void WvWareLegacyMicrosoftFile::open() {
  wvInit();

  int ret = wvInitParser_gsf(&m_ps, m_gsf_input);

  // check if password is required
  if ((ret & 0x8000) != 0) {
    m_encryption_state = EncryptionState::encrypted;
    m_encryption_flag = ret & 0x7fff;

    if ((m_encryption_flag == WORD8) || (m_encryption_flag == WORD7) ||
        (m_encryption_flag == WORD6)) {
      ret = 0;
    }
  } else {
    m_encryption_state = EncryptionState::not_encrypted;
  }

  if (ret != 0) {
    wvOLEFree(&m_ps);
    throw std::runtime_error("wvInitParser failed");
  }
}

std::shared_ptr<abstract::File>
WvWareLegacyMicrosoftFile::file() const noexcept {
  return m_file;
}

FileType WvWareLegacyMicrosoftFile::file_type() const noexcept {
  return FileType::legacy_word_document;
}

FileMeta WvWareLegacyMicrosoftFile::file_meta() const noexcept {
  return {file_type(), password_encrypted(), document_meta()};
}

DecoderEngine WvWareLegacyMicrosoftFile::decoder_engine() const noexcept {
  return DecoderEngine::wvware;
}

DocumentType WvWareLegacyMicrosoftFile::document_type() const {
  return DocumentType::text;
}

DocumentMeta WvWareLegacyMicrosoftFile::document_meta() const { return {}; }

bool WvWareLegacyMicrosoftFile::password_encrypted() const noexcept {
  return m_encryption_state == EncryptionState::encrypted ||
         m_encryption_state == EncryptionState::decrypted;
}

EncryptionState WvWareLegacyMicrosoftFile::encryption_state() const noexcept {
  return m_encryption_state;
}

bool WvWareLegacyMicrosoftFile::decrypt(const std::string &password) {
  if (m_encryption_state != EncryptionState::encrypted) {
    return false;
  }

  wvSetPassword(password.c_str(), &m_ps);

  bool success = false;

  if (m_encryption_flag == WORD8) {
    success = wvDecrypt97(&m_ps);
  } else if (m_encryption_flag == WORD7 || m_encryption_flag == WORD6) {
    success = wvDecrypt95(&m_ps);
  }

  if (!success) {
    return false;
  }

  m_encryption_state = EncryptionState::decrypted;
  return true;
}

std::shared_ptr<abstract::Document>
WvWareLegacyMicrosoftFile::document() const {
  return {}; // TODO throw
}

wvParseStruct &WvWareLegacyMicrosoftFile::parse_struct() const {
  return const_cast<wvParseStruct &>(m_ps);
}

} // namespace odr::internal
