#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/cfb/cfb_file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/oldms/oldms_crypto.hpp>
#include <odr/internal/oldms/oldms_file.hpp>
#include <odr/internal/oldms/presentation/ppt_crypto.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>
#include <odr/internal/oldms/spreadsheet/xls_crypto.hpp>
#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>
#include <odr/internal/oldms/spreadsheet/xls_structs.hpp>
#include <odr/internal/oldms/text/doc_crypto.hpp>
#include <odr/internal/oldms/text/doc_parser.hpp>
#include <odr/internal/oldms/text/doc_structs.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <internal/oldms/oldms_test_util.hpp>
#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::oldms;
using odr::test::TestData;
using odr::test::oldms::append_u16;
using odr::test::oldms::append_u32;
using odr::test::oldms::collect_text;

namespace {

/// A filesystem holding one stream, standing in for the cfb one of a real
/// file — the probes only ever open a stream by name.
VirtualFilesystem filesystem_of(const std::string &path,
                                const std::string &data) {
  VirtualFilesystem result;
  result.copy(std::make_shared<MemoryFile>(data), AbsPath(path));
  return result;
}

/// The streams of a compound file in the test data.
std::shared_ptr<abstract::ReadableFilesystem>
fixture_streams(const std::string &path) {
  return cfb::CfbFile(
             std::make_shared<DiskFile>(TestData::test_file_path(path)))
      .archive()
      ->as_filesystem();
}

/// A `FibBase` ([MS-DOC] 2.5.2) with everything but the flags zeroed.
std::string word_document_stream(const bool encrypted) {
  std::string result;
  append_u16(result, text::fib_wIdent);
  append_u16(result, text::nFib97);
  append_u16(result, 0); // unused
  append_u16(result, 0); // lid
  append_u16(result, 0); // pnNext
  // fDot .. fObfuscated; fEncrypted is bit 8
  append_u16(result, encrypted ? 0x0100 : 0x0000);
  result.resize(sizeof(text::FibBase), '\0');
  return result;
}

/// A `CurrentUserAtom` head ([MS-PPT] 2.3.2).
std::string current_user_stream(const bool encrypted) {
  std::string result;
  append_u16(result, 0); // recVer / recInstance
  append_u16(result, presentation::RT_CurrentUserAtom);
  append_u32(result, 0x14); // recLen
  append_u32(result, 0x14); // size
  append_u32(result, encrypted ? presentation::current_user_token_encrypted
                               : presentation::current_user_token_plain);
  append_u32(result, 0); // offsetToCurrentEdit
  return result;
}

void append_record(std::string &out, const std::uint16_t type,
                   const std::size_t size) {
  append_u16(out, type);
  append_u16(out, static_cast<std::uint16_t>(size));
  out.append(size, '\0');
}

/// A globals substream ([MS-XLS] 2.1.7.20.1) — BOF, then the records the
/// search walks.
std::string workbook_stream(const bool encrypted) {
  std::string result;
  append_record(result, spreadsheet::biff_bof, 16);
  if (encrypted) {
    append_record(result, spreadsheet::biff_filepass, 54);
  }
  append_record(result, spreadsheet::biff_font, 14);
  append_record(result, spreadsheet::biff_eof, 0);
  return result;
}

} // namespace

TEST(OldMsEncryption, doc_reports_the_encrypted_flag) {
  const LegacyMicrosoftFile clear(std::make_shared<VirtualFilesystem>(
      filesystem_of("/WordDocument", word_document_stream(false))));
  EXPECT_TRUE(clear.is_decodable());
  EXPECT_EQ(clear.mimetype(), "application/msword");
  EXPECT_EQ(clear.document_type(), DocumentType::text);
  EXPECT_EQ(text::password_encrypted(
                filesystem_of("/WordDocument", word_document_stream(true))),
            true);
  EXPECT_EQ(text::password_encrypted(
                filesystem_of("/WordDocument", word_document_stream(false))),
            false);
}

TEST(OldMsEncryption, ppt_reports_the_header_token) {
  EXPECT_EQ(presentation::password_encrypted(
                filesystem_of("/Current User", current_user_stream(true))),
            true);
  EXPECT_EQ(presentation::password_encrypted(
                filesystem_of("/Current User", current_user_stream(false))),
            false);
}

TEST(OldMsEncryption, xls_reports_a_file_pass_record) {
  EXPECT_EQ(spreadsheet::password_encrypted(
                filesystem_of("/Workbook", workbook_stream(true))),
            true);
  EXPECT_EQ(spreadsheet::password_encrypted(
                filesystem_of("/Workbook", workbook_stream(false))),
            false);
}

/// Not an answer either way — saying "not encrypted" would be claiming one.
TEST(OldMsEncryption, a_missing_stream_answers_nothing) {
  const VirtualFilesystem empty;

  EXPECT_FALSE(text::password_encrypted(empty).has_value());
  EXPECT_FALSE(presentation::password_encrypted(empty).has_value());
  EXPECT_FALSE(spreadsheet::password_encrypted(empty).has_value());
}

TEST(OldMsEncryption, a_truncated_stream_answers_nothing) {
  EXPECT_FALSE(text::password_encrypted(
                   filesystem_of("/WordDocument",
                                 word_document_stream(true).substr(0, 8)))
                   .has_value());
  EXPECT_FALSE(presentation::password_encrypted(
                   filesystem_of("/Current User",
                                 current_user_stream(true).substr(0, 8)))
                   .has_value());
  EXPECT_FALSE(
      spreadsheet::password_encrypted(
          filesystem_of("/Workbook", workbook_stream(true).substr(0, 3)))
          .has_value());
}

/// A file whose stream says nothing readable keeps `unknown`, which is what the
/// state meant before any of this: the parser then fails on its own terms.
TEST(OldMsEncryption, an_unreadable_stream_leaves_the_state_unknown) {
  const auto files = std::make_shared<VirtualFilesystem>(
      filesystem_of("/WordDocument", "too short"));

  const LegacyMicrosoftFile file(files);

  EXPECT_EQ(file.file_type(), FileType::legacy_word_document);
  EXPECT_FALSE(file.password_encrypted());
  EXPECT_EQ(file.encryption_state(), EncryptionState::unknown);
  EXPECT_TRUE(file.is_decodable());
}

/// The file that reported the issue: it threw
/// "Unexpected negative Fib.ccpText" instead of asking for a password.
TEST(OldMsEncryption, an_encrypted_doc_surfaces_as_encrypted) {
  const DecodedFile file =
      odr::open(TestData::test_file_path("odr-public/doc/encrypted.doc"));

  EXPECT_EQ(file.file_type(), FileType::legacy_word_document);
  EXPECT_TRUE(file.password_encrypted());
  EXPECT_FALSE(file.is_decodable());
  EXPECT_EQ(file.encryption_state(), EncryptionState::encrypted);
  EXPECT_THROW(static_cast<void>(file.as_document_file().document()),
               FileEncryptedError);
}

/// RC4 CryptoAPI and XOR obfuscation from Excel, RC4 from LibreOffice. The
/// expected hashes are of the Workbook stream that msoffcrypto-tool 6.0.0
/// decrypts, which also replaces FilePass with a zero-filled record of type 0.
TEST(OldMsEncryption, xls_decrypts_like_msoffcrypto_tool) {
  struct Case {
    std::string path;
    std::string sha1;
  };
  for (const auto &[path, sha1] : {
           Case{"odr-public/xls/encrypted-rc4-cryptoapi.xls",
                "d0720dddb77495eeb404411eb32cd34146311201"},
           Case{"odr-public/xls/encrypted-xor.xls",
                "5efa756ab3031b5a2e3c79fb9f2f199c318b33f0"},
           Case{"odr-public/xls/encrypted-rc4.xls",
                "dba40ae0a17656be1793892aa5b0bef6b7d43fbb"},
       }) {
    const auto files = fixture_streams(path);
    const std::string password = TestData::test_file(path).password.value();

    EXPECT_FALSE(spreadsheet::decrypt(*files, "wrong").has_value()) << path;
    const std::optional<oldms::crypto::DecryptedStreams> streams =
        spreadsheet::decrypt(*files, password);
    ASSERT_TRUE(streams.has_value()) << path;
    ASSERT_EQ(streams->size(), 1) << path;
    EXPECT_EQ(internal::crypto::util::hex_encode(
                  internal::crypto::util::sha1(streams->front().second)),
              sha1)
        << path;

    const DocumentFile file =
        odr::open(TestData::test_file_path(path)).as_document_file();
    EXPECT_THROW(std::ignore = file.decrypt("wrong"), WrongPasswordError)
        << path;
    const DocumentFile decrypted = file.decrypt(password);
    EXPECT_EQ(decrypted.encryption_state(), EncryptionState::decrypted) << path;
    EXPECT_TRUE(decrypted.document().root_element().first_child().as_sheet())
        << path;
  }
}

/// XOR obfuscation method 1 ([MS-OFFCRYPTO] 2.3.7.1 to 2.3.7.3) for passwords
/// of odd and even length. The expected values come from
/// msoffcrypto-tool 6.0.0.
TEST(OldMsEncryption, xor_method1_like_msoffcrypto_tool) {
  struct Case {
    std::string password;
    std::uint16_t key;
    std::uint16_t verifier;
    std::string xor_array;
  };
  for (const auto &[password, key, verifier, expected] : {
           Case{"a", 0x9d77, 0xce88, "0b134431e6314412fbcee449bb113cce"},
           Case{"ab", 0x69f0, 0xcf03, "c885a54b87e9874ba4f478ebffb4a733"},
           Case{"abcd", 0x2cf6, 0xca5d, "cb27ca24a6e9844b84e9a7567b49fc16"},
           Case{"password", 0x147a, 0x83af, "05ba84b386bd0438e0f5c257c2f5e14a"},
           Case{"123456789012345", 0x8265, 0x9eb1,
                "2a582b5b285a295d2e592a582b5b289c"},
       }) {
    const std::optional<oldms::crypto::XorArray> xor_array =
        oldms::crypto::unlock_xor_method1(key, verifier, password);
    ASSERT_TRUE(xor_array.has_value()) << password;
    EXPECT_EQ(internal::crypto::util::hex_encode(
                  std::string(xor_array->begin(), xor_array->end())),
              expected)
        << password;
    EXPECT_FALSE(oldms::crypto::unlock_xor_method1(key, verifier, "wrong"))
        << password;
  }
}

/// Excel encrypts a workbook with a protected structure with the default
/// password `VelvetSweatshop` ([MS-XLS] 2.2.10); such a file opens without a
/// prompt.
TEST(OldMsEncryption, xls_with_the_default_password_opens_without_a_prompt) {
  const DocumentFile file =
      odr::open(TestData::test_file_path("odr-public/xls/default-password.xls"))
          .as_document_file();

  EXPECT_FALSE(file.password_encrypted());
  EXPECT_EQ(file.encryption_state(), EncryptionState::not_encrypted);
  EXPECT_THROW(std::ignore = file.decrypt("VelvetSweatshop"),
               NotEncryptedError);
  const Document document = file.document();
  const Sheet sheet = document.root_element().first_child().as_sheet();
  EXPECT_EQ(collect_text(sheet.cell(0, 0)), "Secret");
  EXPECT_EQ(collect_text(sheet.cell(1, 0)), "42");
}

TEST(OldMsEncryption, xls_refuses_an_unknown_encryption) {
  const auto workbook = [](const std::string &file_pass) {
    std::string result;
    append_u16(result, 0x002F); // FilePass
    append_u16(result, static_cast<std::uint16_t>(file_pass.size()));
    return result + file_pass;
  };
  std::string unknown_type;
  append_u16(unknown_type, 2); // wEncryptionType
  EXPECT_THROW(std::ignore = spreadsheet::decrypt_workbook(
                   workbook(unknown_type), "password"),
               MsUnsupportedCryptoAlgorithm);

  std::string unknown_version;
  append_u16(unknown_version, 1); // wEncryptionType: RC4
  append_u16(unknown_version, 4); // EncryptionVersionInfo 4.4
  append_u16(unknown_version, 4);
  EXPECT_THROW(std::ignore = spreadsheet::decrypt_workbook(
                   workbook(unknown_version), "password"),
               MsUnsupportedCryptoAlgorithm);
}

/// RC4 from Word and from LibreOffice, RC4 CryptoAPI from Word. The expected
/// hashes are of the streams that msoffcrypto-tool 6.0.0 decrypts: the
/// WordDocument stream, and the Table stream after its encryption header,
/// which msoffcrypto-tool does not keep plain.
TEST(OldMsEncryption, doc_decrypts_like_msoffcrypto_tool) {
  struct Case {
    std::string path;
    std::size_t header_size;
    std::string word_document_sha1;
    std::string table_sha1;
    std::string text;
  };
  for (const auto &[path, header_size, word_document_sha1, table_sha1, text] : {
           Case{"odr-public/doc/encrypted.doc", 52,
                "91f4083c5112a1a1d8f227e6a0af59e3ba86c568",
                "5efbbc93e4183a4254dbe085d8afbacf5410026e", ""},
           Case{"odr-public/doc/encrypted-rc4-cryptoapi.doc", 198,
                "abfc4fbb0b8482304132597b07f79b2007804c78",
                "a23516cd10e61395ce496e8b27f091dd733c8bc4", "lorem ipsum"},
           Case{"odr-public/doc/encrypted-rc4.doc", 52,
                "a754748265c9d77dd8045c897c88a294cd441787",
                "79104f7a0e198476f08797e51953577e608597b5",
                "Secret text in a Word document."},
       }) {
    const auto files = fixture_streams(path);
    const std::string password = TestData::test_file(path).password.value();

    EXPECT_FALSE(text::decrypt(*files, "wrong").has_value()) << path;
    const std::optional<oldms::crypto::DecryptedStreams> streams =
        text::decrypt(*files, password);
    ASSERT_TRUE(streams.has_value()) << path;
    ASSERT_EQ(streams->size(), 2) << path;
    const auto sha1 = [](const std::string_view data) {
      return internal::crypto::util::hex_encode(
          internal::crypto::util::sha1(data));
    };
    EXPECT_EQ(sha1(streams->at(0).second), word_document_sha1) << path;
    EXPECT_EQ(sha1(std::string_view(streams->at(1).second).substr(header_size)),
              table_sha1)
        << path;

    const DocumentFile file =
        odr::open(TestData::test_file_path(path)).as_document_file();
    EXPECT_THROW(std::ignore = file.decrypt("wrong"), WrongPasswordError)
        << path;
    const DocumentFile decrypted = file.decrypt(password);
    EXPECT_EQ(decrypted.encryption_state(), EncryptionState::decrypted) << path;
    const Document document = decrypted.document();
    EXPECT_EQ(collect_text(document.root_element()), text) << path;
  }
}

/// XOR obfuscation method 2 ([MS-DOC] 2.2.6.1) is not read.
TEST(OldMsEncryption, doc_refuses_xor_obfuscation) {
  std::string word_document = word_document_stream(true);
  word_document[11] =
      static_cast<char>(word_document[11] | 0x80); // fObfuscated
  word_document.resize(68, '\0');
  const VirtualFilesystem files = filesystem_of("/WordDocument", word_document);

  EXPECT_THROW(std::ignore = text::decrypt(files, "password"),
               MsUnsupportedCryptoAlgorithm);
}

/// `fWhichTblStm` comes from the file, so the Table stream may be missing.
TEST(OldMsEncryption, doc_without_its_table_stream_is_an_error) {
  std::string word_document = word_document_stream(true);
  word_document.resize(68, '\0');
  const VirtualFilesystem files = filesystem_of("/WordDocument", word_document);

  EXPECT_THROW(std::ignore = text::decrypt(files, "password"),
               std::runtime_error);
}

/// RC4 CryptoAPI from PowerPoint. The expected hash is of the persist objects,
/// in persist id order, that msoffcrypto-tool 6.0.0 decrypts; the
/// CryptSession10Container is plain and not among them.
TEST(OldMsEncryption, ppt_decrypts_like_msoffcrypto_tool) {
  const std::string path = "odr-public/ppt/encrypted-rc4-cryptoapi.ppt";
  const auto files = fixture_streams(path);
  const std::string password = TestData::test_file(path).password.value();

  EXPECT_FALSE(presentation::decrypt(*files, "wrong").has_value());
  std::optional<oldms::crypto::DecryptedStreams> streams =
      presentation::decrypt(*files, password);
  ASSERT_TRUE(streams.has_value());
  std::map<std::string, std::string> by_path;
  for (auto &[stream_path, data] : *streams) {
    by_path.emplace(stream_path.string(), std::move(data));
  }
  const std::string &document = by_path.at("/PowerPoint Document");

  std::istringstream current_user_in(by_path.at("/Current User"));
  std::istringstream document_in(document);
  const presentation::EditChain chain =
      presentation::read_edit_chain(current_user_in, document_in);
  const std::map<std::uint32_t, std::uint32_t> objects(chain.directory.begin(),
                                                       chain.directory.end());
  std::string persist_objects;
  for (const auto &[persist_id, offset] : objects) {
    if (persist_id == chain.encrypt_session_persist_id) {
      continue;
    }
    presentation::RecordHeader header{};
    std::memcpy(&header, document.data() + offset, sizeof(header));
    persist_objects += document.substr(offset, sizeof(header) + header.recLen);
  }
  EXPECT_EQ(internal::crypto::util::hex_encode(
                internal::crypto::util::sha1(persist_objects)),
            "0ce380af3e8f325b0050f842d7c7f8dc6db8cc68");
  EXPECT_EQ(presentation::password_encrypted(
                filesystem_of("/Current User", by_path.at("/Current User"))),
            false);

  const DocumentFile file =
      odr::open(TestData::test_file_path(path)).as_document_file();
  EXPECT_THROW(std::ignore = file.decrypt("wrong"), WrongPasswordError);
  const DocumentFile decrypted = file.decrypt(password);
  const Document document_handle = decrypted.document();
  EXPECT_EQ(collect_text(document_handle.root_element()), "Lorem ipsum");
}

/// Two persist ids at one offset would decrypt the same bytes twice; a crafted
/// directory of many such ids would make the work quadratic.
TEST(OldMsEncryption, ppt_refuses_overlapping_persist_objects) {
  const auto files =
      fixture_streams("odr-public/ppt/encrypted-rc4-cryptoapi.ppt");
  const std::string current_user = internal::util::stream::read(
      *files->open(AbsPath("/Current User"))->stream());
  std::string document = internal::util::stream::read(
      *files->open(AbsPath("/PowerPoint Document"))->stream());

  // CurrentUserAtom.offsetToCurrentEdit, then UserEditAtom
  // .offsetPersistDirectory ([MS-PPT] 2.3.2, 2.3.3)
  std::uint32_t edit = 0;
  std::memcpy(&edit, current_user.data() + 16, sizeof(edit));
  std::uint32_t directory = 0;
  std::memcpy(&directory, document.data() + edit + 8 + 12, sizeof(directory));
  // the first entry's second offset takes the value of its first
  document.replace(directory + 16, 4, document.substr(directory + 12, 4));

  VirtualFilesystem modified;
  modified.copy(std::make_shared<MemoryFile>(current_user),
                AbsPath("/Current User"));
  modified.copy(std::make_shared<MemoryFile>(document),
                AbsPath("/PowerPoint Document"));
  EXPECT_THAT(
      [&] { std::ignore = presentation::decrypt(modified, "Password1234_"); },
      testing::ThrowsMessage<std::runtime_error>(
          testing::HasSubstr("overlapping")));
}

TEST(OldMsEncryption, ppt_refuses_a_persist_length_that_overflows_32_bits) {
  const auto files =
      fixture_streams("odr-public/ppt/encrypted-rc4-cryptoapi.ppt");
  const std::string current_user =
      util::stream::read(*files->open(AbsPath("/Current User"))->stream());
  std::string document = util::stream::read(
      *files->open(AbsPath("/PowerPoint Document"))->stream());
  std::istringstream user_in(current_user);
  std::istringstream document_in(document);
  const presentation::EditChain chain =
      presentation::read_edit_chain(user_in, document_in);
  const std::size_t session =
      chain.directory.at(*chain.encrypt_session_persist_id);
  util::byte_string::Reader reader(std::string_view(document).substr(session));
  const auto session_header = reader.read<presentation::RecordHeader>();
  const auto keys = oldms::crypto::Rc4Encryption(
                        reader.rest().substr(0, session_header.recLen))
                        .unlock("Password1234_");
  ASSERT_TRUE(keys.has_value());
  const std::size_t offset = chain.directory.at(chain.doc_persist_id);
  const std::string key = keys->block_key(chain.doc_persist_id);
  std::string header = internal::crypto::util::rc4(
      key, std::string_view(document).substr(
               offset, sizeof(presentation::RecordHeader)));
  // recLen plus the record header must not wrap on wasm32 or Android armv7.
  const std::uint32_t length = std::numeric_limits<std::uint32_t>::max();
  std::memcpy(header.data() + 4, &length, sizeof(length));
  document.replace(offset, header.size(),
                   internal::crypto::util::rc4(key, header));
  VirtualFilesystem modified;
  modified.copy(std::make_shared<MemoryFile>(current_user),
                AbsPath("/Current User"));
  modified.copy(std::make_shared<MemoryFile>(document),
                AbsPath("/PowerPoint Document"));
  EXPECT_THAT(
      [&] { std::ignore = presentation::decrypt(modified, "Password1234_"); },
      testing::ThrowsMessage<std::runtime_error>(
          testing::HasSubstr("persist object beyond")));
}

/// Apache POI 5.5.1 encrypted these, pictures included. The expected hashes
/// are of the streams that POI decrypts; the Pictures stream is encrypted
/// field by field ([MS-PPT] 2.3.7), and the second file has a picture of each
/// BLIP type with one and with two UIDs.
TEST(OldMsEncryption, ppt_decrypts_pictures_like_apache_poi) {
  struct Case {
    std::string path;
    std::string document_sha1;
    std::string pictures_sha1;
  };
  for (const auto &[path, document_sha1, pictures_sha1] : {
           Case{"odr-public/ppt/encrypted-pictures.ppt",
                "e22fa9b7518ff4aaab111f6c70e98db6ead415c9",
                "81dd6c878d19f9c1df1b11586e140b610d3c5ed5"},
           Case{"odr-public/ppt/encrypted-blip-types.ppt",
                "5436585c07b40b4aa9bc8442c3d1a752803b2d6c",
                "f04e9e3f2ea49b4f62d3f2d351079ee874172320"},
       }) {
    const std::optional<oldms::crypto::DecryptedStreams> streams =
        presentation::decrypt(*fixture_streams(path), "password");
    ASSERT_TRUE(streams.has_value()) << path;
    std::map<std::string, std::string> sha1s;
    for (const auto &[stream_path, data] : *streams) {
      sha1s.emplace(stream_path.string(),
                    internal::crypto::util::hex_encode(
                        internal::crypto::util::sha1(data)));
    }
    EXPECT_EQ(sha1s.at("/PowerPoint Document"), document_sha1) << path;
    EXPECT_EQ(sha1s.at("/Pictures"), pictures_sha1) << path;
  }
}
