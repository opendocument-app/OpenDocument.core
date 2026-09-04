#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <test_util.hpp>

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::test;

TEST(File, open) { EXPECT_THROW(File("/"), FileNotFound); }

TEST(File, from_disk_matches_the_path_constructor) {
  const std::string path = TestData::test_file_path("odr-public/odt/about.odt");

  const File file = File::from_disk(path);

  EXPECT_EQ(file.location(), FileLocation::disk);
  EXPECT_EQ(file.disk_path(), File(path).disk_path());
  EXPECT_EQ(file.size(), File(path).size());
}

/// `open(file, as)` decodes as exactly what it is asked for. A container
/// names its own document type, and `as` is a claim about what is inside it -
/// so a claim the container contradicts is no reading of the file at all.
TEST(File, opening_as_the_wrong_document_type_throws) {
  const struct {
    const char *path;
    FileType is;
    FileType is_not;
  } cases[]{
      {"odr-public/odt/about.odt", FileType::opendocument_text,
       FileType::opendocument_graphics},
      {"odr-public/docx/file-sample_100kB.docx",
       FileType::office_open_xml_document,
       FileType::office_open_xml_presentation},
      {"odr-public/doc/file-sample_100kB.doc", FileType::legacy_word_document,
       FileType::legacy_excel_worksheets},
      // an iwork package names its own app, so asking for the other one is a
      // claim it must refuse rather than answer with what it happens to be
      {"odr-public/pages/empty.pages", FileType::iwork_pages,
       FileType::iwork_keynote},
      {"odr-public/key/empty.key", FileType::iwork_keynote,
       FileType::iwork_pages},
  };

  for (const auto &[path, is, is_not] : cases) {
    const std::string file_path = TestData::test_file_path(path);

    EXPECT_EQ(DecodedFile(file_path, is).file_type(), is) << path;
    EXPECT_THROW(std::ignore = DecodedFile(file_path, is_not), UnknownFileType)
        << path;
  }
}

/// The one reading that is not its own container's: an encrypted ooxml names
/// no inner type until it is decrypted, so it stands in for the one asked for.
TEST(File, an_encrypted_ooxml_opens_as_the_type_asked_for) {
  const DecodedFile file(
      TestData::test_file_path("odr-public/docx/encrypted.docx"),
      FileType::office_open_xml_document);

  EXPECT_EQ(file.file_type(), FileType::office_open_xml_encrypted);
  EXPECT_TRUE(file.password_encrypted());
}

/// The same claim about the same document in its other encoding answers the
/// same way - which is what this fix is about.
TEST(File, a_flat_document_and_a_package_answer_a_wrong_type_alike) {
  const std::string flat =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.text">)"
      R"(<office:body><office:text/></office:body></office:document>)";

  EXPECT_THROW(std::ignore = DecodedFile(File::from_memory(flat),
                                         FileType::opendocument_graphics),
               UnknownFileType);
  EXPECT_THROW(std::ignore = DecodedFile(
                   TestData::test_file_path("odr-public/odt/about.odt"),
                   FileType::opendocument_graphics),
               UnknownFileType);
}

TEST(File, name_is_the_file_name_on_disk) {
  EXPECT_EQ(
      File::from_disk(TestData::test_file_path("odr-public/odt/about.odt"))
          .name(),
      "about.odt");
}

/// Bytes arrive without one, so the caller says what they were called - or
/// nobody does.
TEST(File, from_memory_is_unnamed_unless_told) {
  EXPECT_EQ(File::from_memory("hello").name(), "");
  EXPECT_EQ(File::from_memory("hello", "greeting.txt").name(), "greeting.txt");
}

/// Reading a file into memory drops its path but not what it is called.
TEST(File, a_file_read_into_memory_keeps_its_name) {
  const internal::DiskFile on_disk(
      TestData::test_file_path("odr-public/odt/about.odt"));

  EXPECT_EQ(File(std::make_shared<internal::MemoryFile>(on_disk)).name(),
            "about.odt");
}

/// A file inside an archive is named by its entry, not by the archive.
TEST(File, an_archive_entry_is_named_by_its_entry) {
  internal::zip::ZipArchive zip;
  zip.insert_file(std::end(zip), internal::RelPath("docProps/preview.emf"),
                  std::make_shared<internal::MemoryFile>("not really an emf"));

  std::stringstream out;
  zip.save(out);

  const Filesystem filesystem = DecodedFile(File::from_memory(out.str()))
                                    .as_archive_file()
                                    .archive()
                                    .as_filesystem();

  EXPECT_EQ(filesystem.open("/docProps/preview.emf").name(), "preview.emf");
}

TEST(File, from_memory_holds_its_bytes) {
  const File file = File::from_memory("hello");

  EXPECT_EQ(file.location(), FileLocation::memory);
  EXPECT_EQ(file.size(), 5);
  EXPECT_FALSE(file.disk_path().has_value());
  ASSERT_TRUE(file.memory_data().has_value());
  EXPECT_EQ(*file.memory_data(), "hello");
}

/// The whole point of `from_memory`: a caller with bytes and no path — a
/// download, a browser upload — decodes to exactly what the same bytes on disk
/// would have decoded to.
TEST(File, from_memory_decodes_the_same_as_from_disk) {
  const std::string path = TestData::test_file_path("odr-public/odt/about.odt");

  const DecodedFile from_disk(File::from_disk(path));
  const DecodedFile from_memory(
      File::from_memory(internal::util::file::read(path)));

  EXPECT_EQ(from_memory.file_type(), from_disk.file_type());
  EXPECT_EQ(from_memory.file_category(), from_disk.file_category());
  EXPECT_EQ(from_memory.file_meta().type, from_disk.file_meta().type);
  EXPECT_EQ(from_memory.file_meta().document_type,
            from_disk.file_meta().document_type);

  EXPECT_EQ(DecodedFile::list_file_types(
                File::from_memory(internal::util::file::read(path))),
            DecodedFile::list_file_types(path));
  EXPECT_EQ(DecodedFile::mimetype(
                File::from_memory(internal::util::file::read(path))),
            DecodedFile::mimetype(path));
}

/// `MemoryFile` used to report itself as `disk`, and `memory_data()` handed
/// back a bare pointer that only meant something alongside `size()`.
TEST(File, memory_file_reports_memory_and_its_bytes) {
  const File file(std::make_shared<internal::MemoryFile>(std::string("hello")));

  EXPECT_EQ(file.location(), FileLocation::memory);
  EXPECT_EQ(file.size(), 5);
  EXPECT_FALSE(file.disk_path().has_value());
  ASSERT_TRUE(file.memory_data().has_value());
  EXPECT_EQ(*file.memory_data(), "hello");
}

/// The null file has no bytes anywhere; every other accessor throws.
TEST(File, default_constructed_reports_unknown_location) {
  const File file;

  EXPECT_EQ(file.location(), FileLocation::unknown);
  EXPECT_THROW(std::ignore = file.size(), NullPointerError);
  EXPECT_THROW(std::ignore = file.stream(), NullPointerError);
}

TEST(File, disk_file_has_no_memory_data) {
  const File file(std::make_shared<internal::DiskFile>(
      TestData::test_file_path("odr-public/odt/about.odt")));

  EXPECT_EQ(file.location(), FileLocation::disk);
  EXPECT_TRUE(file.disk_path().has_value());
  EXPECT_FALSE(file.memory_data().has_value());
}

TEST(DocumentFile, open) { EXPECT_THROW(DocumentFile("/"), FileNotFound); }

TEST(DocumentFile, from_disk_and_from_memory_agree) {
  const std::string path = TestData::test_file_path("odr-public/odt/about.odt");

  const DocumentFile from_disk = DocumentFile::from_disk(path);
  const DocumentFile from_memory =
      DocumentFile::from_memory(internal::util::file::read(path));

  EXPECT_EQ(from_memory.file_type(), from_disk.file_type());
  EXPECT_EQ(from_memory.document_type(), from_disk.document_type());
  EXPECT_EQ(from_memory.document().document_type(),
            from_disk.document().document_type());
}

/// Not a document, so both factories have to refuse it the same way.
TEST(DocumentFile, from_memory_throws_on_a_non_document) {
  EXPECT_THROW(std::ignore = DocumentFile::from_memory("not a document"),
               NoDocumentFile);
}

TEST(DocumentFile, odf_thumbnail) {
  const DocumentFile file(
      TestData::test_file_path("odr-public/ods/file_example_ODS_10.ods"));

  const std::optional<File> thumbnail = file.thumbnail();
  ASSERT_TRUE(thumbnail.has_value());
  EXPECT_LT(0, thumbnail->size());
  EXPECT_EQ(DecodedFile(*thumbnail).file_type(),
            FileType::portable_network_graphics);
}

TEST(DocumentFile, thumbnail_is_absent_where_the_package_has_none) {
  const DocumentFile file(
      TestData::test_file_path("odr-public/docx/style-various-1.docx"));

  EXPECT_FALSE(file.thumbnail().has_value());
}

/// The name is deliberately unconventional: only reading the relationship
/// finds it.
TEST(DocumentFile, ooxml_thumbnail_is_named_by_the_package_relationship) {
  internal::zip::ZipArchive zip;
  zip.insert_file(
      std::end(zip), internal::RelPath("_rels/.rels"),
      std::make_shared<internal::MemoryFile>(
          R"(<?xml version="1.0"?>)"
          R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
          R"(<Relationship Id="rId1" Target="docProps/preview.emf" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail"/>)"
          R"(</Relationships>)"));
  zip.insert_file(std::end(zip), internal::RelPath("word/document.xml"),
                  std::make_shared<internal::MemoryFile>(
                      R"(<?xml version="1.0"?><w:document )"
                      R"(xmlns:w="http://schemas.openxmlformats.org/)"
                      R"(wordprocessingml/2006/main"><w:body/></w:document>)"));
  zip.insert_file(std::end(zip), internal::RelPath("docProps/preview.emf"),
                  std::make_shared<internal::MemoryFile>("not really an emf"));

  std::stringstream out;
  zip.save(out);

  const DocumentFile file = DocumentFile::from_memory(out.str());
  ASSERT_EQ(file.file_type(), FileType::office_open_xml_document);

  const std::optional<File> thumbnail = file.thumbnail();
  ASSERT_TRUE(thumbnail.has_value());
  EXPECT_EQ(internal::util::stream::read(*thumbnail->stream()),
            "not really an emf");
}

TEST(DecodedFile, wpd) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path =
      TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd");
  try {
    DecodedFile file(path, logger);
    FAIL();
  } catch (const UnsupportedFileType &e) {
    EXPECT_EQ(e.file_type, FileType::word_perfect);
  }
}
