// `odr::ErrorCode` crosses every binding as a number a host switches on, so a
// code that moves is a wrong message on a screen, not a build failure.
// Appending is silent; renumbering fails here.

#include <odr/error_code.hpp>
#include <odr/exceptions.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <stdexcept>

using namespace odr;

namespace {

/// One of every exception type the header declares.
std::vector<std::pair<ErrorCode, const char *>> thrown_codes() {
  const auto of = [](const std::exception &e) { return error_code(e); };
  return {
      {of(UnsupportedOperation()), "UnsupportedOperation"},
      {of(FileNotFound()), "FileNotFound"},
      {of(UnknownFileType()), "UnknownFileType"},
      {of(FileReadError()), "FileReadError"},
      {of(FileWriteError("p")), "FileWriteError"},
      {of(NoZipFile()), "NoZipFile"},
      {of(ZipSaveError()), "ZipSaveError"},
      {of(CfbError("d")), "CfbError"},
      {of(NoCfbFile()), "NoCfbFile"},
      {of(CfbFileCorrupted()), "CfbFileCorrupted"},
      {of(NoTextFile()), "NoTextFile"},
      {of(NoCsvFile()), "NoCsvFile"},
      {of(NoMarkdownFile()), "NoMarkdownFile"},
      {of(NoJsonFile()), "NoJsonFile"},
      {of(NoImageFile()), "NoImageFile"},
      {of(NoArchiveFile()), "NoArchiveFile"},
      {of(NoDocumentFile()), "NoDocumentFile"},
      {of(NoOpenDocumentFile()), "NoOpenDocumentFile"},
      {of(NoOfficeOpenXmlFile()), "NoOfficeOpenXmlFile"},
      {of(NoPdfFile()), "NoPdfFile"},
      {of(NoFontFile()), "NoFontFile"},
      {of(NoLegacyMicrosoftFile()), "NoLegacyMicrosoftFile"},
      {of(NoIworkFile()), "NoIworkFile"},
      {of(NoXmlFile()), "NoXmlFile"},
      {of(NoSvgFile()), "NoSvgFile"},
      {of(NoRtfFile()), "NoRtfFile"},
      {of(UnsupportedCryptoAlgorithm()), "UnsupportedCryptoAlgorithm"},
      {of(NoSvmFile()), "NoSvmFile"},
      {of(MalformedSvmFile()), "MalformedSvmFile"},
      {of(UnsupportedEndian()), "UnsupportedEndian"},
      {of(MsUnsupportedCryptoAlgorithm()), "MsUnsupportedCryptoAlgorithm"},
      {of(UnknownDocumentType()), "UnknownDocumentType"},
      {of(ValueNotStated()), "ValueNotStated"},
      {of(InvalidPrefix()), "InvalidPrefix"},
      {of(DocumentCopyProtectedException()), "DocumentCopyProtected"},
      {of(ResourceNotAccessible()), "ResourceNotAccessible"},
      {of(PrefixInUse()), "PrefixInUse"},
      {of(ServerBindFailed("h", 1)), "ServerBindFailed"},
      {of(ServerAlreadyBound()), "ServerAlreadyBound"},
      {of(ServerNotBound()), "ServerNotBound"},
      {of(UnsupportedOption("o")), "UnsupportedOption"},
      {of(NullPointerError("v")), "NullPointerError"},
      {of(WrongPasswordError()), "WrongPassword"},
      {of(DecryptionFailed()), "DecryptionFailed"},
      {of(NotEncryptedError()), "NotEncrypted"},
      {of(InvalidPath("p")), "InvalidPath"},
      {of(UnsupportedFileEncoding("e")), "UnsupportedFileEncoding"},
      {of(FileEncryptedError()), "FileEncrypted"},
      {of(UnauthenticatedReadError()), "UnauthenticatedReadError"},
  };
}

} // namespace

/// Moving one of these breaks an installed iOS app, which no build would
/// notice.
TEST(ErrorCode, apple_head_is_pinned) {
  EXPECT_EQ(static_cast<int>(ErrorCode::unknown), 1);
  EXPECT_EQ(static_cast<int>(ErrorCode::unsupported_operation), 2);
  EXPECT_EQ(static_cast<int>(ErrorCode::file_not_found), 3);
  EXPECT_EQ(static_cast<int>(ErrorCode::unknown_file_type), 4);
  EXPECT_EQ(static_cast<int>(ErrorCode::unsupported_file_type), 5);
  EXPECT_EQ(static_cast<int>(ErrorCode::file_read_error), 6);
  EXPECT_EQ(static_cast<int>(ErrorCode::file_write_error), 7);
  EXPECT_EQ(static_cast<int>(ErrorCode::no_document_file), 8);
  EXPECT_EQ(static_cast<int>(ErrorCode::unknown_document_type), 9);
  EXPECT_EQ(static_cast<int>(ErrorCode::unsupported_crypto_algorithm), 10);
  EXPECT_EQ(static_cast<int>(ErrorCode::wrong_password), 11);
  EXPECT_EQ(static_cast<int>(ErrorCode::decryption_failed), 12);
  EXPECT_EQ(static_cast<int>(ErrorCode::not_encrypted), 13);
  EXPECT_EQ(static_cast<int>(ErrorCode::file_encrypted), 14);
  EXPECT_EQ(static_cast<int>(ErrorCode::document_copy_protected), 15);
}

/// What the editing scripts raise.
TEST(ErrorCode, edit_band_is_pinned) {
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_new_line), 1001);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_formula), 1002);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_rich), 1003);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_shapes), 1004);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_read_only), 1005);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_formula_input), 1006);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_unsupported), 1007);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_range), 1008);
  EXPECT_EQ(static_cast<int>(ErrorCode::edit_unnameable), 1009);
}

TEST(ErrorCode, every_code_is_named_once) {
  std::set<ErrorCode> seen;
  std::set<std::string_view> names;
  for (const ErrorCode code : all_error_codes()) {
    EXPECT_TRUE(seen.insert(code).second)
        << "duplicate code " << static_cast<int>(code);
    const std::string_view name = error_code_name(code);
    EXPECT_FALSE(name.empty());
    EXPECT_TRUE(names.insert(name).second) << "duplicate name " << name;
  }
}

/// So no binding needs a catch ladder to recover one.
TEST(ErrorCode, every_exception_states_its_code) {
  for (const auto &[code, name] : thrown_codes()) {
    EXPECT_NE(code, ErrorCode::unknown) << name << " states no code";
    EXPECT_EQ(error_code_name(code), name);
  }
}

TEST(ErrorCode, unknown_for_anything_else) {
  EXPECT_EQ(error_code(std::runtime_error("not ours")), ErrorCode::unknown);
  EXPECT_EQ(error_code(Exception("the bare base")), ErrorCode::unknown);
}

TEST(ErrorCode, bands_do_not_overlap) {
  for (const auto &[code, name] : thrown_codes()) {
    EXPECT_LT(code, ErrorCode::edit_new_line) << name << " is in the edit band";
  }
}
