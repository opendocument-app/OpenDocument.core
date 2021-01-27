#include <cfb/cfb_impl.h>
#include <cfb/cfb_util.h>
#include <codecvt>
#include <locale>

namespace odr::cfb::util {

std::string name_to_string(const std::uint16_t *name, std::size_t length) {
  static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
      convert;
  return convert.to_bytes(std::u16string(
      reinterpret_cast<const char16_t *>(name), (length - 1) / 2));
}

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
