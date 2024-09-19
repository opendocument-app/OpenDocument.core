#include <odr/internal/oldms_wvware/wvware_oldms_file.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>

#include <memory>
#include <utility>

#include <wv/wv.h>

namespace odr::internal {

WvWareLegacyMicrosoftFile::WvWareLegacyMicrosoftFile(
    std::shared_ptr<common::DiskFile> file)
    : m_file{std::move(file)} {
  wvInit();
  char *path = const_cast<char *>(m_file->disk_path()->string().c_str());
  int ret = wvInitParser(&m_ps, path);

  // check if password is required
  if ((ret & 0x8000) != 0) {
    m_encryption_state = EncryptionState::encrypted;
    m_encryption_flag = ret & 0x7fff;

    if ((m_encryption_flag == WORD8) || (m_encryption_flag == WORD7) ||
        (m_encryption_flag == WORD6)) {
      ret = 0;
    }
  } else {
    m_encryption_state = EncryptionState::decrypted;
  }

  if (ret != 0) {
    wvOLEFree(&m_ps);
    throw std::runtime_error("wvInitParser failed");
  }
}

WvWareLegacyMicrosoftFile::~WvWareLegacyMicrosoftFile() { wvOLEFree(&m_ps); }

std::shared_ptr<abstract::File>
WvWareLegacyMicrosoftFile::file() const noexcept {
  return m_file;
}

FileType WvWareLegacyMicrosoftFile::file_type() const noexcept {
  return {}; // TODO
}

FileMeta WvWareLegacyMicrosoftFile::file_meta() const noexcept {
  return {}; // TODO
}

DecoderEngine WvWareLegacyMicrosoftFile::decoder_engine() const noexcept {
  return DecoderEngine::wv_ware;
}

DocumentType WvWareLegacyMicrosoftFile::document_type() const {
  return {}; // TODO
}

DocumentMeta WvWareLegacyMicrosoftFile::document_meta() const {
  return {}; // TODO
}

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
