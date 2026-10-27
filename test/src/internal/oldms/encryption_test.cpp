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
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>
#include <odr/internal/oldms/spreadsheet/xls_crypto.hpp>
#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>
#include <odr/internal/oldms/spreadsheet/xls_structs.hpp>
#include <odr/internal/oldms/text/doc_parser.hpp>
#include <odr/internal/oldms/text/doc_structs.hpp>

#include <internal/oldms/oldms_test_util.hpp>
#include <test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
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
