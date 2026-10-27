#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/iwork/iwork_crypto.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/zip/zip_file.hpp>

#include <internal/iwork/iwork_element_util.hpp>
#include <internal/oldms/oldms_test_util.hpp>
#include <test_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace odr;
using odr::test::TestData;
namespace builder = odr::test::iwork;
namespace iwork = odr::internal::iwork;

namespace {

std::shared_ptr<internal::abstract::ReadableFilesystem>
package(const std::string &path) {
  return internal::zip::ZipFile(std::make_shared<internal::DiskFile>(
                                    TestData::test_file_path(path)))
      .archive()
      ->as_filesystem();
}

} // namespace

/// Pages, Numbers and Keynote 15.3.1 with a password: the type before
/// decryption comes from the member names, and the document after it.
TEST(IworkCrypto, decrypts_pages_numbers_and_keynote) {
  struct Case {
    std::string path;
    FileType type;
    std::string text;
  };
  for (const auto &[path, type, text] : {
           Case{"odr-public/pages/encrypted.pages", FileType::iwork_pages,
                "Secret text in a Pages document."},
           Case{"odr-public/numbers/encrypted.numbers", FileType::iwork_numbers,
                ""},
           Case{"odr-public/key/encrypted.key", FileType::iwork_keynote,
                "Secret text in a Keynote document."},
       }) {
    const DocumentFile file =
        open(TestData::test_file_path(path)).as_document_file();
    EXPECT_EQ(file.file_type(), type) << path;
    EXPECT_TRUE(file.password_encrypted()) << path;
    EXPECT_EQ(file.encryption_state(), EncryptionState::encrypted) << path;
    EXPECT_THROW(std::ignore = file.document(), FileEncryptedError) << path;
    EXPECT_THROW(std::ignore = file.decrypt("wrong"), WrongPasswordError)
        << path;

    const DocumentFile decrypted = file.decrypt("password");
    EXPECT_EQ(decrypted.encryption_state(), EncryptionState::decrypted) << path;
    EXPECT_EQ(decrypted.file_type(), type) << path;
    const Document document = decrypted.document();
    if (type == FileType::iwork_numbers) {
      const Sheet sheet = document.root_element().first_child().as_sheet();
      EXPECT_EQ(builder::element_text(sheet.cell(0, 0)),
                "Secret text in a Numbers document.")
          << path;
    } else {
      EXPECT_EQ(odr::test::oldms::collect_text(document.root_element()), text)
          << path;
    }
  }
}

/// The password is UTF-8 in NFC.
TEST(IworkCrypto, decrypts_with_a_unicode_password) {
  const DocumentFile file =
      open(TestData::test_file_path(
               "odr-public/pages/encrypted-unicode-password.pages"))
          .as_document_file();
  EXPECT_THROW(std::ignore = file.decrypt("Tr0ub4dor&3 aou"),
               WrongPasswordError);
  EXPECT_NO_THROW(std::ignore = file.decrypt("Tr0ub4dor&3 äöü"));
}

/// A member with more than 1 MiB of payload is split into segments: the
/// 2 MiB image comes out whole.
TEST(IworkCrypto, decrypts_a_segmented_member) {
  const auto decrypted = iwork::decrypt(
      package("odr-public/pages/encrypted-large-image.pages"), "password");
  ASSERT_TRUE(decrypted.has_value());
  const std::shared_ptr<internal::abstract::File> image =
      (*decrypted)->open(internal::AbsPath("/Data/data-24.png"));
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(internal::crypto::util::hex_encode(internal::crypto::util::sha1(
                internal::util::stream::read(*image->stream()))),
            "0690daec17d475aa501d72efe38bd8b322633d2b");
  EXPECT_FALSE((*decrypted)->is_file(internal::AbsPath("/.iwpv2")));
}

/// The HMAC of each segment catches a damaged member.
TEST(IworkCrypto, refuses_a_damaged_member) {
  const auto files = package("odr-public/pages/encrypted.pages");
  const auto decrypted = iwork::decrypt(files, "password");
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_NO_THROW(
      std::ignore =
          (*decrypted)->open(internal::AbsPath("/Index/Document.iwa")));

  std::string data = internal::util::stream::read(
      *files->open(internal::AbsPath("/Index/Document.iwa"))->stream());
  data[100] = static_cast<char>(data[100] ^ 1);
  // the key is derived again from the verifier
  const std::string verifier = internal::util::stream::read(
      *files->open(internal::AbsPath("/.iwpv2"))->stream());
  const std::string key = internal::crypto::util::pbkdf2(
      16, "password", verifier.substr(8, 16), 100000);
  EXPECT_THROW(std::ignore = iwork::decrypt_member(data, key, "password"),
               std::runtime_error);
}

/// The iteration count comes from the file.
TEST(IworkCrypto, refuses_an_excessive_iteration_count) {
  std::string verifier(104, '\0');
  verifier[0] = 2; // version
  verifier[2] = 1; // format
  verifier.replace(4, 4, "\xff\xff\xff\xff");
  const auto files = std::make_shared<internal::VirtualFilesystem>();
  files->copy(std::make_shared<internal::MemoryFile>(verifier),
              internal::AbsPath("/.iwpv2"));

  EXPECT_THROW(std::ignore = iwork::decrypt(files, "password"),
               std::runtime_error);
}
