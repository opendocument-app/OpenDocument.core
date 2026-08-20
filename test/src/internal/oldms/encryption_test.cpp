#include <odr/document.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>
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

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::oldms;
using odr::test::TestData;
using odr::test::oldms::append_u16;
using odr::test::oldms::append_u32;

namespace {

/// A filesystem holding one stream, standing in for the cfb one of a real
/// file — the probes only ever open a stream by name.
VirtualFilesystem filesystem_of(const std::string &path,
                                const std::string &data) {
  VirtualFilesystem result;
  result.copy(std::make_shared<MemoryFile>(data), AbsPath(path));
  return result;
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
  EXPECT_TRUE(text::password_encrypted(
      filesystem_of("/WordDocument", word_document_stream(true))));
  EXPECT_FALSE(text::password_encrypted(
      filesystem_of("/WordDocument", word_document_stream(false))));
}

TEST(OldMsEncryption, ppt_reports_the_header_token) {
  EXPECT_TRUE(presentation::password_encrypted(
      filesystem_of("/Current User", current_user_stream(true))));
  EXPECT_FALSE(presentation::password_encrypted(
      filesystem_of("/Current User", current_user_stream(false))));
}

TEST(OldMsEncryption, xls_reports_a_file_pass_record) {
  EXPECT_TRUE(spreadsheet::password_encrypted(
      filesystem_of("/Workbook", workbook_stream(true))));
  EXPECT_FALSE(spreadsheet::password_encrypted(
      filesystem_of("/Workbook", workbook_stream(false))));
}

TEST(OldMsEncryption, a_missing_stream_is_not_encrypted) {
  const VirtualFilesystem empty;

  EXPECT_FALSE(text::password_encrypted(empty));
  EXPECT_FALSE(presentation::password_encrypted(empty));
  EXPECT_FALSE(spreadsheet::password_encrypted(empty));
}

TEST(OldMsEncryption, a_truncated_stream_is_not_encrypted) {
  EXPECT_FALSE(text::password_encrypted(
      filesystem_of("/WordDocument", word_document_stream(true).substr(0, 8))));
  EXPECT_FALSE(presentation::password_encrypted(
      filesystem_of("/Current User", current_user_stream(true).substr(0, 8))));
  EXPECT_FALSE(spreadsheet::password_encrypted(
      filesystem_of("/Workbook", workbook_stream(true).substr(0, 3))));
}

/// The file that reported the issue: it threw
/// "Unexpected negative Fib.ccpText" instead of asking for a password.
TEST(OldMsEncryption, an_encrypted_doc_surfaces_as_encrypted) {
  const DecodedFile file =
      odr::open(TestData::test_file_path("odr-public/doc/encrypted.doc"));

  EXPECT_EQ(file.file_type(), FileType::legacy_word_document);
  EXPECT_TRUE(file.password_encrypted());
  EXPECT_EQ(file.encryption_state(), EncryptionState::encrypted);
  EXPECT_THROW(static_cast<void>(file.as_document_file().document()),
               FileEncryptedError);
}
