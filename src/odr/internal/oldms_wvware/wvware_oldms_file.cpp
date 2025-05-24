#include <odr/internal/oldms_wvware/wvware_oldms_file.hpp>

#include <odr/internal/common/file.hpp>

#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-input-stdio.h>
#include <wv/wv.h>

namespace odr::internal {

struct WvWareLegacyMicrosoftFile::ParserState {
  GsfInput *gsf_input{};

  wvParseStruct ps{};
  int encryption_flag{};

  ~ParserState() { wvOLEFree(&ps); }
};

WvWareLegacyMicrosoftFile::WvWareLegacyMicrosoftFile(
    std::shared_ptr<common::DiskFile> file)
    : m_file{std::move(file)} {
  GError *error = nullptr;

  m_parser_state = std::make_shared<ParserState>();

  m_parser_state->gsf_input =
      gsf_input_stdio_new(m_file->disk_path()->string().c_str(), &error);

  if (m_parser_state->gsf_input == nullptr) {
    throw std::runtime_error("gsf_input_stdio_new failed");
  }

  open();
}

WvWareLegacyMicrosoftFile::WvWareLegacyMicrosoftFile(
    std::shared_ptr<common::MemoryFile> file)
    : m_file{std::move(file)} {
  m_parser_state = std::make_shared<ParserState>();

  m_parser_state->gsf_input = gsf_input_memory_new(
      reinterpret_cast<const guint8 *>(m_file->memory_data()),
      static_cast<gsf_off_t>(m_file->size()), false);

  open();
}

void WvWareLegacyMicrosoftFile::open() {
  wvInit();

  int ret = wvInitParser_gsf(&m_parser_state->ps, m_parser_state->gsf_input);

  // check if password is required
  if ((ret & 0x8000) != 0) {
    m_encryption_state = EncryptionState::encrypted;
    m_parser_state->encryption_flag = ret & 0x7fff;

    if ((m_parser_state->encryption_flag == WORD8) ||
        (m_parser_state->encryption_flag == WORD7) ||
        (m_parser_state->encryption_flag == WORD6)) {
      ret = 0;
    }
  } else {
    m_encryption_state = EncryptionState::not_encrypted;
  }

  if (ret != 0) {
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

  wvSetPassword(password.c_str(), &m_parser_state->ps);

  bool success = false;

  if (m_parser_state->encryption_flag == WORD8) {
    success = wvDecrypt97(&m_parser_state->ps) == 0;
  } else if (m_parser_state->encryption_flag == WORD7 ||
             m_parser_state->encryption_flag == WORD6) {
    success = wvDecrypt95(&m_parser_state->ps) == 0;
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
  return m_parser_state->ps;
}

} // namespace odr::internal
