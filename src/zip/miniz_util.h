#ifndef ODR_ZIP_MINIZ_UTIL_H
#define ODR_ZIP_MINIZ_UTIL_H

#include <chrono>
#include <istream>
#include <memory>
#include <miniz.h>
#include <string>

namespace odr::abstract {
class File;
}

namespace odr::common {
class MemoryFile;
class DiscFile;
} // namespace odr::common

namespace odr::zip::miniz {

class Archive final {
public:
  explicit Archive(const std::shared_ptr<common::MemoryFile> &file);
  explicit Archive(const std::shared_ptr<common::DiscFile> &file);
  Archive(const Archive &);
  Archive(Archive &&) noexcept;
  ~Archive();
  Archive &operator=(const Archive &);
  Archive &operator=(Archive &&) noexcept;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const;

private:
  mz_zip_archive m_zip{};
  std::shared_ptr<abstract::File> m_file;
  std::unique_ptr<std::istream> m_data;

  explicit Archive(std::shared_ptr<abstract::File> file);

  void init_();
};

// TODO encapsulate `mz_zip_reader_extract_iter_state`?

class ReaderBuffer final : public std::streambuf {
public:
  explicit ReaderBuffer(mz_zip_reader_extract_iter_state *iter);
  ReaderBuffer(mz_zip_reader_extract_iter_state *iter, std::size_t buffer_size);
  ~ReaderBuffer() final;

  int underflow() final;

private:
  mz_zip_reader_extract_iter_state *m_iter{};
  std::uint64_t m_remaining{0};
  std::size_t m_buffer_size{4098};
  char *m_buffer;
};

bool append_file(mz_zip_archive &archive, const std::string &path,
                 std::istream &istream, std::size_t size,
                 const std::time_t &time, const std::string &comment,
                 std::uint32_t level_and_flags);

} // namespace odr::zip::miniz

#endif // ODR_ZIP_MINIZ_UTIL_H
