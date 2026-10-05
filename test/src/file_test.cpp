#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/random.hpp>
#include <odr/internal/common/temporary_file.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <test_util.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

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

/// `open(file, DecodeOptions::as(as))` decodes as exactly what it is asked for.
/// A container names its own document type, and `as` is a claim about what is
/// inside it - so a claim the container contradicts is no reading of the file
/// at all.
TEST(File, opening_as_the_wrong_document_type_throws) {
  struct Case {
    const char *path;
    FileType is;
    FileType is_not;
  };
  const std::array cases{
      Case{"odr-public/odt/about.odt", FileType::opendocument_text,
           FileType::opendocument_graphics},
      Case{"odr-public/docx/file-sample_100kB.docx",
           FileType::office_open_xml_document,
           FileType::office_open_xml_presentation},
      Case{"odr-public/doc/file-sample_100kB.doc",
           FileType::legacy_word_document, FileType::legacy_excel_worksheets},
      // an iwork package names its own app, so asking for the other one is a
      // claim it must refuse rather than answer with what it happens to be
      Case{"odr-public/pages/empty.pages", FileType::iwork_pages,
           FileType::iwork_keynote},
      Case{"odr-public/key/empty.key", FileType::iwork_keynote,
           FileType::iwork_pages},
  };

  for (const auto &[path, is, is_not] : cases) {
    const std::string file_path = TestData::test_file_path(path);

    EXPECT_EQ(open(file_path, DecodeOptions::as(is)).file_type(), is) << path;
    // the engine that refuses says so in its own words, and which engine that
    // is varies by row - `Exception` is the shared base
    EXPECT_THROW(std::ignore = open(file_path, DecodeOptions::as(is_not)),
                 Exception)
        << path;
  }
}

/// The one reading that is not its own container's: an encrypted ooxml names
/// no inner type until it is decrypted, so it stands in for the one asked for.
TEST(File, an_encrypted_ooxml_opens_as_the_type_asked_for) {
  const DecodedFile file =
      open(TestData::test_file_path("odr-public/docx/encrypted.docx"),
           DecodeOptions::as(FileType::office_open_xml_document));

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

  EXPECT_THROW(std::ignore =
                   open(File::from_memory(flat),
                        DecodeOptions::as(FileType::opendocument_graphics)),
               NoOpenDocumentFile);
  EXPECT_THROW(std::ignore =
                   open(TestData::test_file_path("odr-public/odt/about.odt"),
                        DecodeOptions::as(FileType::opendocument_graphics)),
               NoOpenDocumentFile);
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

TEST(File, copying_into_memory_uses_one_size_snapshot) {
  class ChangingFile final : public internal::abstract::File {
  public:
    FileLocation location() const noexcept override {
      return FileLocation::memory;
    }
    std::size_t size() const override { return ++size_reads == 1 ? 4 : 2; }
    std::string name() const override { return "changing"; }
    std::optional<internal::AbsPath> disk_path() const override { return {}; }
    std::optional<std::string_view> memory_data() const override { return {}; }
    std::unique_ptr<std::istream> stream() const override {
      return std::make_unique<std::istringstream>("data");
    }
    mutable std::uint32_t size_reads{0};
  } source;

  const internal::MemoryFile copy(source);
  EXPECT_EQ(copy.content(), "data");
  EXPECT_EQ(source.size_reads, 1);
}

TEST(File, temporary_copies_preserve_binary_bytes) {
  const std::string data("a\r\nb\nc\0d", 8);
  const internal::MemoryFile source(data);
  const auto copy =
      internal::TemporaryDiskFileFactory::system_default().copy(source);
  EXPECT_EQ(internal::MemoryFile(copy).content(), data);
}

TEST(File, text_can_be_saved_over_its_disk_source) {
  std::istringstream source("original text\n");
  const auto temporary =
      internal::TemporaryDiskFileFactory::system_default().copy(source);
  const std::string path = temporary.disk_path()->string();
  const TextFile text =
      open(path, DecodeOptions::as(FileType::text_file)).as_text_file();
  text.save(path);
  EXPECT_EQ(internal::util::file::read(path), "original text\n");
}

TEST(File, failed_temporary_copies_leave_no_file) {
  const internal::AbsPath directory(std::filesystem::temp_directory_path());
  const std::string name = "odr-failed-copy-" + internal::random_string(12);
  const internal::TemporaryDiskFileFactory factory(directory,
                                                   [name] { return name; });
  std::istringstream source("bytes");
  source.setstate(std::ios::badbit);
  EXPECT_THROW(std::ignore = factory.copy(source), std::ios_base::failure);
  EXPECT_FALSE(
      std::filesystem::exists(directory.join(internal::RelPath(name)).path()));
}

/// A file inside an archive is named by its entry, not by the archive.
TEST(File, an_archive_entry_is_named_by_its_entry) {
  internal::zip::ZipArchive zip;
  zip.insert_file(std::end(zip), internal::RelPath("docProps/preview.emf"),
                  std::make_shared<internal::MemoryFile>("not really an emf"));

  std::stringstream out;
  zip.save(out);

  const Filesystem filesystem = open(File::from_memory(out.str()))
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

  const DecodedFile from_disk = open(File::from_disk(path));
  const DecodedFile from_memory =
      open(File::from_memory(internal::util::file::read(path)));

  EXPECT_EQ(from_memory.file_type(), from_disk.file_type());
  EXPECT_EQ(from_memory.file_category(), from_disk.file_category());
  EXPECT_EQ(from_memory.file_meta().type, from_disk.file_meta().type);
  EXPECT_EQ(from_memory.file_meta().document_type,
            from_disk.file_meta().document_type);

  EXPECT_EQ(
      list_file_types(File::from_memory(internal::util::file::read(path))),
      list_file_types(path));
  EXPECT_EQ(mimetype(File::from_memory(internal::util::file::read(path))),
            mimetype(path));
}

/// The null file has no bytes anywhere; every other accessor throws.
TEST(File, default_constructed_reports_unknown_location) {
  const File file;

  EXPECT_EQ(file.location(), FileLocation::unknown);
  EXPECT_THROW(std::ignore = file.size(), NullPointerError);
  EXPECT_THROW(std::ignore = file.stream(), NullPointerError);
}

TEST(File, opening_or_probing_an_empty_handle_throws) {
  const File file;
  EXPECT_THROW(std::ignore = open(file), NullPointerError);
  EXPECT_THROW(std::ignore = open(file, DecodeOptions::as(FileType::text_file)),
               NullPointerError);
  EXPECT_THROW(std::ignore = list_file_types(file), NullPointerError);
  EXPECT_THROW(std::ignore = mimetype(file), NullPointerError);
}

TEST(File, disk_file_has_no_memory_data) {
  const File file(std::make_shared<internal::DiskFile>(
      TestData::test_file_path("odr-public/odt/about.odt")));

  EXPECT_EQ(file.location(), FileLocation::disk);
  EXPECT_TRUE(file.disk_path().has_value());
  EXPECT_FALSE(file.memory_data().has_value());
}

TEST(DocumentFile, from_disk_and_from_memory_agree) {
  const std::string path = TestData::test_file_path("odr-public/odt/about.odt");

  const DocumentFile from_disk = open(path).as_document_file();
  const DocumentFile from_memory =
      open(File::from_memory(internal::util::file::read(path)))
          .as_document_file();

  EXPECT_EQ(from_memory.file_type(), from_disk.file_type());
  EXPECT_EQ(from_memory.document_type(), from_disk.document_type());
  EXPECT_EQ(from_memory.document().document_type(),
            from_disk.document().document_type());
}

/// Not a document, so both factories have to refuse it the same way.
TEST(DocumentFile, from_memory_throws_on_a_non_document) {
  EXPECT_THROW(std::ignore =
                   open(File::from_memory("not a document")).as_document_file(),
               NoDocumentFile);
}

TEST(DocumentFile, odf_thumbnail) {
  const DocumentFile file =
      open(TestData::test_file_path("odr-public/ods/file_example_ODS_10.ods"))
          .as_document_file();

  const std::optional<File> thumbnail = file.thumbnail();
  ASSERT_TRUE(thumbnail.has_value());
  EXPECT_LT(0, thumbnail->size());
  EXPECT_EQ(open(*thumbnail).file_type(), FileType::portable_network_graphics);
}

TEST(DocumentFile, thumbnail_is_absent_where_the_package_has_none) {
  const DocumentFile file =
      open(TestData::test_file_path("odr-public/docx/style-various-1.docx"))
          .as_document_file();

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

  const DocumentFile file =
      open(File::from_memory(out.str())).as_document_file();
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
    DecodedFile file = open(path, {}, logger);
    FAIL();
  } catch (const UnsupportedFileType &e) {
    EXPECT_EQ(e.file_type, FileType::word_perfect);
  }
}

TEST(File, temporary_names_never_replace_existing_files) {
  std::istringstream original("original");
  const auto existing =
      internal::TemporaryDiskFileFactory::system_default().copy(original);
  const internal::AbsPath path = existing.disk_path().value();
  std::uint32_t attempts = 0;
  const internal::TemporaryDiskFileFactory factory(path.parent(), [&] {
    if (++attempts > 1000) {
      throw std::runtime_error("unbounded name retries");
    }
    return existing.name();
  });
  std::istringstream replacement("replacement");
  EXPECT_THROW(std::ignore = factory.copy(replacement), FileWriteError);
  EXPECT_EQ(internal::MemoryFile(existing).content(), "original");
}

TEST(File, temporary_moves_transfer_cleanup_ownership) {
  std::string first_path;
  std::string second_path;
  {
    std::istringstream source("bytes");
    auto first =
        internal::TemporaryDiskFileFactory::system_default().copy(source);
    first_path = first.disk_path()->string();
    auto moved = std::move(first);
    EXPECT_TRUE(std::filesystem::exists(first_path));
    std::istringstream other("other");
    auto second =
        internal::TemporaryDiskFileFactory::system_default().copy(other);
    second_path = second.disk_path()->string();
    second = std::move(moved);
    EXPECT_FALSE(std::filesystem::exists(second_path));
    EXPECT_EQ(internal::MemoryFile(second).content(), "bytes");
  }
  EXPECT_FALSE(std::filesystem::exists(first_path));
}

TEST(File, temporary_creation_does_not_follow_a_dangling_symlink) {
  const auto directory = std::filesystem::temp_directory_path();
  const std::string name = "odr-symlink-" + internal::random_string(12);
  const auto link = directory / name;
  const auto target = directory / (name + "-target");
  std::error_code error;
  std::filesystem::create_symlink(target, link, error);
  if (error) {
    GTEST_SKIP() << "symlink creation unavailable: " << error.message();
  }
  const internal::TemporaryDiskFileFactory factory(internal::AbsPath(directory),
                                                   [name] { return name; });
  std::istringstream source("bytes");
  EXPECT_THROW(std::ignore = factory.copy(source), FileWriteError);
  EXPECT_FALSE(std::filesystem::exists(target));
  std::filesystem::remove(link, error);
  std::filesystem::remove(target, error);
}

TEST(File, metadata_failures_propagate_to_the_caller) {
  class FailingMetadata final : public internal::abstract::DecodedFile {
  public:
    std::shared_ptr<internal::abstract::File> file() const noexcept override {
      return {};
    }
    FileType file_type() const noexcept override { return FileType::unknown; }
    FileCategory file_category() const noexcept override {
      return FileCategory::unknown;
    }
    std::string_view mimetype() const noexcept override { return {}; }
    FileMeta file_meta() const override { throw std::bad_alloc(); }
    bool is_decodable() const noexcept override { return false; }
  };
  const DecodedFile file(std::make_shared<FailingMetadata>());
  EXPECT_THROW((void)file.file_meta(), std::bad_alloc);
}

TEST(File, atomic_writes_preserve_the_destination_on_failure) {
  using namespace odr::internal;
  const AbsPath directory(std::filesystem::current_path());
  const std::string name = "odr-atomic-" + random_string(12);
  const std::filesystem::path path = directory.join(RelPath(name)).path();
  util::file::write_atomic(name, [](std::ostream &out) { out << "original"; });
  const TemporaryDiskFile cleanup(path.string());
  for (const bool throws : {false, true}) {
    EXPECT_ANY_THROW(util::file::write_atomic(name, [&](std::ostream &out) {
      out << "partial";
      if (throws) {
        out.exceptions(std::ios::badbit);
      }
      out.setstate(std::ios::badbit);
    }));
    EXPECT_EQ(util::file::read(name), "original");
  }
  const auto permissions = std::filesystem::status(path).permissions();
  util::file::write_atomic(name,
                           [](std::ostream &out) { out << "replacement"; });
  EXPECT_EQ(util::file::read(name), "replacement");
  EXPECT_EQ(std::filesystem::status(path).permissions(), permissions);
}

TEST(File, atomic_writes_follow_existing_symlinks_and_reject_dangling_ones) {
  using namespace odr::internal;
  const auto target = TemporaryDiskFileFactory::system_default().create(
      [](std::ostream &out) { out << "original"; });
  const std::string link_path = target.path().string() + ".link";
  std::error_code error;
  std::filesystem::create_symlink(target.path().path(), link_path, error);
  if (error) {
    GTEST_SKIP() << "Symlinks unavailable: " << error.message();
  }
  const TemporaryDiskFile link(link_path);
  util::file::write_atomic(link.path().string(),
                           [](std::ostream &out) { out << "replacement"; });
  EXPECT_TRUE(std::filesystem::is_symlink(link.path().path()));
  EXPECT_EQ(util::file::read(target.path().string()), "replacement");
  std::filesystem::remove(target.path().path());
  EXPECT_THROW(
      util::file::write_atomic(link.path().string(),
                               [](std::ostream &out) { out << "lost"; }),
      FileWriteError);
  EXPECT_TRUE(std::filesystem::is_symlink(link.path().path()));
}
