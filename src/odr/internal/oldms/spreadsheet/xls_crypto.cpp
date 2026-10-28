#include <odr/internal/oldms/spreadsheet/xls_crypto.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/spreadsheet/xls_structs.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <algorithm>
#include <span>
#include <stdexcept>
#include <utility>

namespace odr::internal::oldms::spreadsheet {

namespace {

/// [MS-XLS] 2.1.4: a record header, type and size.
constexpr std::size_t header_size = 4;

/// [MS-XLS] 2.2.10: the RC4 key changes every 1024 bytes.
constexpr std::size_t rc4_block_size = 1024;

struct Record {
  std::uint16_t type{0};
  std::size_t body{0};
  std::size_t size{0};
};

/// The record at `position`, or nothing where the stream ends.
std::optional<Record> record_at(const std::string_view workbook,
                                const std::size_t position) {
  if (position + header_size > workbook.size()) {
    return std::nullopt;
  }
  util::byte_string::Reader reader(workbook.substr(position, header_size));
  Record result;
  result.type = reader.read<std::uint16_t>();
  result.size = reader.read<std::uint16_t>();
  result.body = position + header_size;
  if (result.body + result.size > workbook.size()) {
    return std::nullopt;
  }
  return result;
}

/// The FilePass record of the globals substream.
Record find_file_pass(const std::string_view workbook) {
  for (std::optional<Record> record = record_at(workbook, 0);
       record.has_value() && record->type != biff_eof;
       record = record_at(workbook, record->body + record->size)) {
    if (record->type == biff_filepass) {
      return *record;
    }
  }
  throw std::runtime_error("xls: no FilePass record");
}

/// The offset in the body from which a record is encrypted, or nothing if it
/// is not ([MS-XLS] 2.2.10).
std::optional<std::size_t> encrypted_from(const Record &record) {
  switch (record.type) {
  case biff_bof:
  case biff_filepass:
  case biff_usrexcl:
  case biff_filelock:
  case biff_interfacehdr:
  case biff_rrdinfo:
  case biff_rrdhead:
    return std::nullopt;
  case biff_boundsheet:
    // lbPlyPos
    return 4;
  default:
    return 0;
  }
}

} // namespace

} // namespace odr::internal::oldms::spreadsheet

namespace odr::internal::oldms {

std::optional<std::string>
spreadsheet::decrypt_workbook(std::string workbook,
                              const std::string_view password) {
  const Record file_pass = find_file_pass(workbook);
  util::byte_string::Reader reader(
      std::string_view(workbook).substr(file_pass.body, file_pass.size));

  // [MS-XLS] 2.4.117: XOR obfuscation or RC4.
  std::optional<crypto::XorArray> xor_array;
  std::string keystream;
  if (const auto encryption_type = reader.read<std::uint16_t>();
      encryption_type == 0) {
    // [MS-XLS] 2.5.343.
    const auto key = reader.read<std::uint16_t>();
    const auto verifier = reader.read<std::uint16_t>();
    xor_array = crypto::unlock_xor_method1(key, verifier, password);
    if (!xor_array.has_value()) {
      return std::nullopt;
    }
  } else if (encryption_type == 1) {
    const std::optional<crypto::Rc4Keys> keys =
        crypto::Rc4Encryption(reader.rest()).unlock(password);
    if (!keys.has_value()) {
      return std::nullopt;
    }
    keystream = keys->keystream(workbook.size(), rc4_block_size);
  } else {
    throw MsUnsupportedCryptoAlgorithm();
  }

  for (std::optional<Record> record = record_at(workbook, 0);
       record.has_value();
       record = record_at(workbook, record->body + record->size)) {
    const std::optional<std::size_t> from = encrypted_from(*record);
    if (!from.has_value() || *from >= record->size) {
      continue;
    }
    const std::size_t begin = record->body + *from;
    const std::size_t end = record->body + record->size;
    if (xor_array.has_value()) {
      // The XOR array index continues from where the encrypted bytes end, and
      // BoundSheet8 adds the 4 bytes of its lbPlyPos.
      crypto::decrypt_xor_method1(
          *xor_array, end + *from,
          std::span(workbook).subspan(begin, end - begin));
    } else {
      for (std::size_t i = begin; i < end; ++i) {
        workbook[i] = static_cast<char>(workbook[i] ^ keystream[i]);
      }
    }
  }

  // A zero-filled record of type 0 in place of the FilePass record.
  std::fill_n(workbook.begin() +
                  static_cast<std::ptrdiff_t>(file_pass.body - header_size),
              2, '\0');
  std::fill_n(workbook.begin() + static_cast<std::ptrdiff_t>(file_pass.body),
              file_pass.size, '\0');
  return workbook;
}

std::optional<crypto::DecryptedStreams>
spreadsheet::decrypt(const abstract::ReadableFilesystem &files,
                     const std::string_view password) {
  const AbsPath path("/Workbook");
  std::optional<std::string> workbook = decrypt_workbook(
      util::stream::read(*files.open(path)->stream()), password);
  if (!workbook.has_value()) {
    return std::nullopt;
  }
  return crypto::DecryptedStreams{{path, std::move(*workbook)}};
}

} // namespace odr::internal::oldms
