#ifndef ODR_ZIP_MINIZ_UTIL_H
#define ODR_ZIP_MINIZ_UTIL_H

#include <chrono>
#include <istream>
#include <miniz.h>
#include <string>

namespace odr::zip::miniz {

class ReaderBuffer : public std::streambuf {
public:
  explicit ReaderBuffer(mz_zip_reader_extract_iter_state *iter);
  ReaderBuffer(mz_zip_reader_extract_iter_state *iter, std::size_t buffer_size);
  ~ReaderBuffer() override;

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
