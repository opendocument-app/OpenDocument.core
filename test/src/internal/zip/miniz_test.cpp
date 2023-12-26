#include <miniz/miniz.h>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <string>

using namespace odr::test;

TEST(miniz, list) {
  mz_zip_archive zip{};
  bool state = mz_zip_reader_init_file(
      &zip,
      TestData::test_file_path("odr-public/odt/style-various-1.odt").c_str(),
      0);
  EXPECT_TRUE(state);

  auto num_files = mz_zip_reader_get_num_files(&zip);
  for (std::uint32_t i = 0; i < num_files; ++i) {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(&zip, i, &stat);
  }

  EXPECT_EQ(19, num_files);
}

TEST(miniz, create) {
  bool state;
  std::ofstream out("test.zip");
  const auto time =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  mz_zip_archive archive{};
  archive.m_pIO_opaque = &out;
  archive.m_pWrite = [](void *opaque, std::uint64_t offset, const void *buffer,
                        std::size_t size) {
    auto ostream = static_cast<std::ostream *>(opaque);
    EXPECT_EQ(offset, ostream->tellp());
    ostream->write(static_cast<const char *>(buffer), size);
    return size;
  };
  state = mz_zip_writer_init(&archive, 0);
  EXPECT_TRUE(state);

  auto append_file = [&](const char *path, const std::string &content) {
    auto read_callback = [](void *opaque, std::uint64_t /*offset*/,
                            void *buffer, std::size_t size) -> std::size_t {
      auto istream = static_cast<std::istream *>(opaque);
      istream->read(static_cast<char *>(buffer), size);
      return istream->gcount();
    };

    std::istringstream istream(content);
    auto size = content.size();

    state = mz_zip_writer_add_read_buf_callback(&archive, path, read_callback,
                                                &istream, size, &time, nullptr,
                                                0, 6, nullptr, 0, nullptr, 0);
    EXPECT_TRUE(state);
  };

  auto append_directory = [&](const char *path) {
    state = mz_zip_writer_add_mem(&archive, path, nullptr, 0, 0);
    EXPECT_TRUE(state);
  };

  append_directory("a/");
  append_directory("b/");
  append_file("text", "hello world");
  append_file("apple", "i am an apple");

  state = mz_zip_writer_finalize_archive(&archive);
  EXPECT_TRUE(state);
  state = mz_zip_writer_end(&archive);
  EXPECT_TRUE(state);
}
