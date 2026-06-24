#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/pdf/pdf_filesystem.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

using odr::File;
using odr::Filesystem;
using odr::HtmlConfig;
using odr::HtmlService;
using namespace odr::internal;      // MemoryFile, util
using namespace odr::internal::pdf; // PdfFile, create_object_filesystem
using PdfFileBuilder = odr::test::pdf::PdfFileBuilder;

namespace {

// A minimal four-object PDF: catalog, page tree, page, and one stream object
// (the page content). Object ids are 1..4 in insertion order.
PdfFile make_pdf() {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << >> /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .trailer("/Root 1 0 R");
  return PdfFile(std::make_shared<MemoryFile>(builder.build_classic()));
}

std::string read(const File &file) {
  const std::unique_ptr<std::istream> stream = file.stream();
  return util::stream::read(*stream);
}

} // namespace

TEST(PdfFilesystem, lists_objects_and_streams) {
  const Filesystem filesystem = create_object_filesystem(make_pdf());

  EXPECT_TRUE(filesystem.is_directory("/objects"));
  EXPECT_TRUE(filesystem.is_directory("/streams"));
  EXPECT_TRUE(filesystem.is_file("/trailer"));

  for (const char *object :
       {"/objects/1_0", "/objects/2_0", "/objects/3_0", "/objects/4_0"}) {
    EXPECT_TRUE(filesystem.is_file(object)) << object;
  }

  // only the content object (4) carries a stream
  EXPECT_TRUE(filesystem.is_file("/streams/4_0"));
  EXPECT_FALSE(filesystem.exists("/streams/1_0"));
}

TEST(PdfFilesystem, object_content_is_the_serialized_value) {
  const Filesystem filesystem = create_object_filesystem(make_pdf());

  const std::string catalog = read(filesystem.open("/objects/1_0"));
  EXPECT_NE(catalog.find("/Catalog"), std::string::npos);

  const std::string trailer = read(filesystem.open("/trailer"));
  EXPECT_NE(trailer.find("/Root"), std::string::npos);
}

TEST(PdfFilesystem, stream_content_is_decoded_bytes) {
  const Filesystem filesystem = create_object_filesystem(make_pdf());

  EXPECT_EQ(read(filesystem.open("/streams/4_0")), "BT ET");
}

TEST(PdfFilesystem, renders_through_the_filesystem_viewer) {
  const Filesystem filesystem = create_object_filesystem(make_pdf());

  HtmlConfig config;
  const std::string cache = std::filesystem::temp_directory_path().string();
  const HtmlService service = odr::html::translate(filesystem, cache, config);

  std::ostringstream out;
  service.write("files.html", out);
  EXPECT_NE(out.str().find("/objects/1_0"), std::string::npos);
}
