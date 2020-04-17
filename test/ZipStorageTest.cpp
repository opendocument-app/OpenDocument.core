#include <access/ZipStorage.h>
#include <gtest/gtest.h>
#include <string>

// TODO visit test

TEST(ZipReader, exception) {
  EXPECT_THROW(odr::access::ZipReader("/"), odr::access::NoZipFileException);
}

TEST(ZipWriter, create) {
  const std::string file = "created.zip";

  {
    odr::access::ZipWriter writer(file);

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
  }

  {
    odr::access::ZipReader reader(file);
    EXPECT_TRUE(reader.isFile("one.txt"));
    EXPECT_TRUE(reader.isFile("two.txt"));
    EXPECT_TRUE(reader.isDirectory("empty"));
    EXPECT_TRUE(reader.isDirectory("notempty"));
    EXPECT_TRUE(reader.isFile("notempty/three.txt"));

    EXPECT_EQ(23, reader.size("one.txt"));
    EXPECT_EQ(28, reader.size("two.txt"));
    EXPECT_EQ(4, reader.size("notempty/three.txt"));
  }
}

TEST(ZipWriter, create_order) {
  const std::string file = "created.zip";
  const std::vector<std::string> entries{"z", "one", "two", "three", "a", "0"};

  {
    odr::access::ZipWriter writer(file);

    for (auto &&e : entries) {
      writer.write(e);
    }
  }

  {
    odr::access::ZipReader reader(file);

    auto it = entries.begin();
    reader.visit([&](const auto &path) {
      EXPECT_EQ(*it, path.string());
      ++it;
    });
  }
}

// TODO copy test
