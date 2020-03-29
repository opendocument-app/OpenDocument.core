#include <access/ZipStorage.h>
#include <gtest/gtest.h>
#include <string>

// TODO visit test

TEST(ZipReader, exception) {
  EXPECT_THROW(odr::access::ZipReader reader("/"),
               odr::access::NoZipFileException);
}

TEST(ZipWriter, create) {
  const std::string output = "created.zip";

  odr::access::ZipWriter writer(output);

  {
    const auto sink = writer.write("one.txt");
    sink->write("this is written at once", 23);
  }

  {
    const auto sink = writer.write("two.txt");
    sink->write("this is written ", 15);
    sink->write("at two stages", 13);
  }
}

// TODO copy test
