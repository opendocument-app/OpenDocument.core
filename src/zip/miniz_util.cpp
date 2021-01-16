#include <zip/miniz_util.h>

namespace odr::zip {

namespace miniz {
ReaderBuffer::ReaderBuffer(mz_zip_reader_extract_iter_state *iter)
    : ReaderBuffer(iter, 4098) {}

ReaderBuffer::ReaderBuffer(mz_zip_reader_extract_iter_state *iter,
                           const std::size_t buffer_size)
    : m_iter{iter}, m_remaining{iter->file_stat.m_uncomp_size},
      m_buffer_size{buffer_size}, m_buffer{new char[m_buffer_size]} {}

ReaderBuffer::~ReaderBuffer() {
  mz_zip_reader_extract_iter_free(m_iter);
  delete[] m_buffer;
}

int ReaderBuffer::underflow() {
  if (m_remaining <= 0) {
    return std::char_traits<char>::eof();
  }

  const std::uint64_t amount = std::min(m_remaining, m_buffer_size);
  const std::uint32_t result =
      mz_zip_reader_extract_iter_read(m_iter, m_buffer, amount);
  m_remaining -= result;
  this->setg(this->m_buffer, this->m_buffer, this->m_buffer + result);

  return std::char_traits<char>::to_int_type(*gptr());
}
} // namespace miniz

bool miniz::append_file(mz_zip_archive &archive, const std::string &path,
                        std::istream &istream, const std::size_t size,
                        const std::time_t &time, const std::string &comment,
                        const std::uint32_t level_and_flags) {
  auto read_callback = [](void *opaque, std::uint64_t /*offset*/, void *buffer,
                          std::size_t size) {
    auto istream = static_cast<std::istream *>(opaque);
    istream->read(static_cast<char *>(buffer), size);
    return size;
  };

  return mz_zip_writer_add_read_buf_callback(
      &archive, path.c_str(), read_callback, &istream, size, &time,
      comment.c_str(), comment.size(), level_and_flags, nullptr, 0, nullptr, 0);
}

} // namespace odr::zip
