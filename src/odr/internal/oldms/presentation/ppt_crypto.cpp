#include <odr/internal/oldms/presentation/ppt_crypto.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace odr::internal::oldms::presentation {

namespace {

/// The persist object at `offset`, decrypted with the key of its persist id
/// ([MS-PPT] 2.3.7). The record header is encrypted too, so it is decrypted
/// first to learn the length.
void decrypt_persist_object(std::string &document, const std::uint32_t offset,
                            const std::string &key) {
  if (offset > document.size() ||
      document.size() - offset < sizeof(RecordHeader)) {
    throw std::runtime_error("ppt: persist object beyond the stream");
  }
  const std::string header = internal::crypto::util::rc4(
      key, std::string_view(document).substr(offset, sizeof(RecordHeader)));
  RecordHeader record_header{};
  std::memcpy(&record_header, header.data(), sizeof(record_header));
  const std::size_t size = sizeof(RecordHeader) + record_header.recLen;
  if (document.size() - offset < size) {
    throw std::runtime_error("ppt: persist object beyond the stream");
  }
  const std::string plaintext = internal::crypto::util::rc4(
      key, std::string_view(document).substr(offset, size));
  document.replace(offset, size, plaintext);
}

/// [MS-ODRAW] 2.2.23 and 2.2.32: the record types of the Pictures stream.
constexpr std::uint16_t fbse_type = 0xF007;
constexpr std::uint16_t first_blip_type = 0xF018;
constexpr std::uint16_t last_blip_type = 0xF117;
/// EMF, WMF and PICT carry a 34-byte metafile header, the others a tag byte.
constexpr std::uint16_t first_metafile_type = 0xF01A;
constexpr std::uint16_t last_metafile_type = 0xF01C;
constexpr std::size_t metafile_header_size = 34;

/// [MS-ODRAW] 2.2.24 to 2.2.31: the recInstance of a BLIP with a second UID.
bool has_second_uid(const std::uint16_t instance) {
  switch (instance) {
  case 0x217:
  case 0x3D5:
  case 0x46B:
  case 0x543:
  case 0x6E1:
  case 0x6E3:
  case 0x6E5:
  case 0x7A9:
    return true;
  default:
    return false;
  }
}

/// [MS-PPT] 2.3.7 and 4: each field of a record in the Pictures stream is
/// encrypted on its own with the key of block 0. Which fields there are
/// follows Apache POI's `HSLFSlideShowEncrypted`.
void decrypt_pictures(std::string &pictures, const std::string &key) {
  std::size_t position = 0;
  std::size_t end = 0;
  const auto field = [&](const std::size_t size) {
    if (end - position < size) {
      throw std::runtime_error("ppt: picture field beyond its record");
    }
    pictures.replace(
        position, size,
        internal::crypto::util::rc4(
            key, std::string_view(pictures).substr(position, size)));
    position += size;
  };
  const auto header_field = [&] {
    field(sizeof(RecordHeader));
    RecordHeader header{};
    std::memcpy(&header, pictures.data() + position - sizeof(header),
                sizeof(header));
    return header;
  };

  while (pictures.size() - position >= sizeof(RecordHeader)) {
    end = pictures.size();
    RecordHeader header = header_field();
    if (header.recType != fbse_type &&
        (header.recType < first_blip_type || header.recType > last_blip_type)) {
      break;
    }
    if (pictures.size() - position < header.recLen) {
      throw std::runtime_error("ppt: picture record beyond the stream");
    }
    end = position + header.recLen;

    if (header.recType == fbse_type) {
      // [MS-ODRAW] 2.2.32: btWin32 to unused3; cbName ends at byte 34
      const std::size_t body = position;
      for (const std::size_t size : {1, 1, 16, 2, 4, 4, 4, 1, 1, 1, 1}) {
        field(size);
      }
      const auto name_size = static_cast<std::uint8_t>(pictures[body + 33]);
      if (name_size > 0) {
        field(name_size);
      }
      if (position == end) {
        continue;
      }
      header = header_field();
    }

    field(16); // rgbUid1
    if (has_second_uid(header.recInstance)) {
      field(16); // rgbUid2
    }
    field(header.recType >= first_metafile_type &&
                  header.recType <= last_metafile_type
              ? metafile_header_size
              : 1);
    field(end - position);
  }
}

} // namespace

} // namespace odr::internal::oldms::presentation

namespace odr::internal::oldms {

std::optional<crypto::DecryptedStreams>
presentation::decrypt(const abstract::ReadableFilesystem &files,
                      const std::string_view password) {
  const AbsPath current_user_path("/Current User");
  const AbsPath document_path("/PowerPoint Document");
  std::string current_user =
      util::stream::read(*files.open(current_user_path)->stream());
  std::string document =
      util::stream::read(*files.open(document_path)->stream());

  std::istringstream current_user_in(current_user);
  std::istringstream document_in(document);
  const EditChain chain = read_edit_chain(current_user_in, document_in);
  if (!chain.encrypt_session_persist_id.has_value()) {
    throw std::runtime_error("ppt: no encryptSessionPersistIdRef");
  }
  const auto session = chain.directory.find(*chain.encrypt_session_persist_id);
  if (session == chain.directory.end() || session->second > document.size() ||
      document.size() - session->second < sizeof(RecordHeader)) {
    throw std::runtime_error("ppt: no CryptSession10Container");
  }

  // [MS-PPT] 2.3.7: the CryptSession10Container is plain.
  RecordHeader session_header{};
  std::memcpy(&session_header, document.data() + session->second,
              sizeof(session_header));
  if (session_header.recType != RT_CryptSession10Container ||
      document.size() - session->second - sizeof(RecordHeader) <
          session_header.recLen) {
    throw std::runtime_error("ppt: bad CryptSession10Container");
  }
  const std::optional<crypto::Rc4Keys> keys =
      crypto::Rc4Encryption(
          std::string_view(document).substr(
              session->second + sizeof(RecordHeader), session_header.recLen))
          .unlock(password);
  if (!keys.has_value()) {
    return std::nullopt;
  }

  for (const auto &[persist_id, offset] : chain.directory) {
    if (persist_id != session->first) {
      decrypt_persist_object(document, offset, keys->block_key(persist_id));
    }
  }

  // [MS-PPT] 2.3.2: headerToken follows the record header and `size`.
  std::string token;
  util::byte_string::put_u32_le(token, current_user_token_plain);
  current_user.replace(sizeof(RecordHeader) + sizeof(std::uint32_t),
                       token.size(), token);

  crypto::DecryptedStreams result;
  result.emplace_back(current_user_path, std::move(current_user));
  result.emplace_back(document_path, std::move(document));

  const AbsPath pictures_path("/Pictures");
  if (files.is_file(pictures_path)) {
    std::string pictures =
        util::stream::read(*files.open(pictures_path)->stream());
    decrypt_pictures(pictures, keys->block_key(0));
    result.emplace_back(pictures_path, std::move(pictures));
  }
  return result;
}

} // namespace odr::internal::oldms
