#include <common/path.h>
#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>
#include <zip/zip_storage.h>

using namespace odr::zip;

// TODO visit test

TEST(ZipReader, exception) { EXPECT_THROW(ZipReader("/"), odr::NoZipFile); }

TEST(ZipWriter, create) {
  const std::string file = "created.zip";

  {
    ZipWriter writer(file);

    {
      const auto sink = writer.write("one.txt");
      sink->write("this is written at once", 23);
    }

    {
      const auto sink = writer.write("two.txt");
      sink->write("this is written ", 15);
      sink->write("at two stages", 13);
    }

    writer.createDirectory("empty");
    writer.createDirectory("notempty");

    {
      const auto sink = writer.write("notempty/three.txt");
      sink->write("asdf", 4);
    }

    {
      const auto sink = writer.write("./notempty/four.txt");
      sink->write("1234", 4);
    }
  }

  {
    ZipReader reader(file);

    EXPECT_TRUE(reader.isFile("one.txt"));
    EXPECT_TRUE(reader.isFile("two.txt"));
    EXPECT_TRUE(reader.isDirectory("empty"));
    EXPECT_TRUE(reader.isDirectory("notempty"));
    EXPECT_TRUE(reader.isFile("notempty/three.txt"));
    EXPECT_TRUE(reader.isFile("notempty/four.txt"));
    EXPECT_TRUE(reader.isFile("./notempty/four.txt"));

    EXPECT_EQ(23, reader.size("one.txt"));
    EXPECT_EQ(28, reader.size("two.txt"));
    EXPECT_EQ(4, reader.size("notempty/three.txt"));
    EXPECT_EQ(4, reader.size("notempty/four.txt"));
    EXPECT_EQ(4, reader.size("./notempty/four.txt"));
  }
}

TEST(ZipWriter, create_order) {
  const std::string file = "created.zip";
  const std::vector<std::string> entries{"z", "one", "two", "three", "a", "0"};

  {
    ZipWriter writer(file);

    for (auto &&e : entries) {
      writer.write(e);
    }
  }

  {
    ZipReader reader(file);

    auto it = entries.begin();
    reader.visit([&](const auto &path) {
      EXPECT_EQ(*it, path.string());
      ++it;
    });
  }
}

// TODO copy test
