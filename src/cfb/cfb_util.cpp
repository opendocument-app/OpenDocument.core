#include <cfb/cfb_impl.h>
#include <cfb/cfb_util.h>

namespace odr::cfb::util {

ReaderBuffer::ReaderBuffer(const impl::CompoundFileReader &reader,
                           const impl::CompoundFileEntry &entry)
    : m_reader{reader}, m_entry{entry}, m_buffer{new char[m_buffer_size]} {}

ReaderBuffer::~ReaderBuffer() { delete[] m_buffer; }

int ReaderBuffer::underflow() {
  const std::uint64_t remaining = m_entry.size - m_offset;
  if (remaining <= 0) {
    return std::char_traits<char>::eof();
  }

  const std::uint64_t amount = std::min(remaining, m_buffer_size);
  m_reader.read_file(&m_entry, m_offset, m_buffer, amount);
  m_offset += amount;
  setg(m_buffer, m_buffer, m_buffer + amount);

  return std::char_traits<char>::to_int_type(*gptr());
}

} // namespace odr::cfb::util
