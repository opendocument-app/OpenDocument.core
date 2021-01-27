#ifndef ODR_CFB_UTIL_H
#define ODR_CFB_UTIL_H

#include <memory>
#include <streambuf>

namespace odr::cfb::impl {
class CompoundFileReader;
struct CompoundFileEntry;
} // namespace odr::cfb::impl

namespace odr::cfb::util {

std::string name_to_string(const std::uint16_t *name, std::size_t length);

class ReaderBuffer : public std::streambuf {
public:
  ReaderBuffer(const impl::CompoundFileReader &reader,
               const impl::CompoundFileEntry &entry);
  ~ReaderBuffer() override;

  int underflow() final;

private:
  const impl::CompoundFileReader &m_reader;
  const impl::CompoundFileEntry &m_entry;
  std::uint64_t m_offset{0};
  std::size_t m_buffer_size{4098};
  char *m_buffer;
};

} // namespace odr::cfb::util

#endif // ODR_CFB_UTIL_H
