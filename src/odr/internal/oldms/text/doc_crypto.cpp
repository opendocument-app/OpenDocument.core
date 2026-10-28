#include <odr/internal/oldms/text/doc_crypto.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/text/doc_structs.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

namespace odr::internal::oldms::text {

namespace {

/// [MS-DOC] 2.2.6.2: the first 68 bytes of the WordDocument stream stay plain.
constexpr std::size_t plain_word_document_size = 68;

/// [MS-DOC] 2.2.6.2: the key changes every 512 bytes.
constexpr std::size_t rc4_block_size = 512;

/// XORs `stream` from `from` on with the keystream, which starts at the
/// beginning of the stream.
void decrypt_stream(std::string &stream, const std::size_t from,
                    const crypto::Rc4Keys &keys) {
  const std::string keystream = keys.keystream(stream.size(), rc4_block_size);
  for (std::size_t i = from; i < stream.size(); ++i) {
    stream[i] = static_cast<char>(stream[i] ^ keystream[i]);
  }
}

} // namespace

} // namespace odr::internal::oldms::text

namespace odr::internal::oldms {

std::optional<crypto::DecryptedStreams>
text::decrypt(const abstract::ReadableFilesystem &files,
              const std::string_view password) {
  const AbsPath word_document_path("/WordDocument");
  std::string word_document =
      util::stream::read(*files.open(word_document_path)->stream());
  if (word_document.size() < plain_word_document_size) {
    throw std::runtime_error("doc: WordDocument stream too short");
  }
  auto base = util::byte_string::Reader(word_document).read<FibBase>();
  if (base.fObfuscated != 0) {
    // XOR obfuscation method 2 ([MS-DOC] 2.2.6.1)
    throw MsUnsupportedCryptoAlgorithm();
  }

  const AbsPath table_path(base.fWhichTblStm == 1 ? "/1Table" : "/0Table");
  const std::shared_ptr<abstract::File> table_file = files.open(table_path);
  if (table_file == nullptr) {
    throw std::runtime_error("doc: no Table stream");
  }
  std::string table = util::stream::read(*table_file->stream());
  if (base.lKey > table.size()) {
    throw std::runtime_error("doc: encryption header beyond the Table stream");
  }

  // [MS-DOC] 2.2.6.2 and 2.2.6.3: the header is the first lKey bytes.
  const std::optional<crypto::Rc4Keys> keys =
      crypto::Rc4Encryption(std::string_view(table).substr(0, base.lKey))
          .unlock(password);
  if (!keys.has_value()) {
    return std::nullopt;
  }

  decrypt_stream(word_document, plain_word_document_size, *keys);
  decrypt_stream(table, base.lKey, *keys);

  base.fEncrypted = 0;
  base.fObfuscated = 0;
  base.lKey = 0;
  std::memcpy(word_document.data(), &base, sizeof(base));

  crypto::DecryptedStreams result;
  result.emplace_back(word_document_path, std::move(word_document));
  result.emplace_back(table_path, std::move(table));

  const AbsPath data_path("/Data");
  if (files.is_file(data_path)) {
    std::string data = util::stream::read(*files.open(data_path)->stream());
    decrypt_stream(data, 0, *keys);
    result.emplace_back(data_path, std::move(data));
  }
  return result;
}

} // namespace odr::internal::oldms
