#include <odr/internal/oldms/spreadsheet/xls_io.hpp>

#include <odr/internal/oldms/spreadsheet/xls_structs.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <istream>
#include <stdexcept>

namespace odr::internal::oldms::spreadsheet {

BiffReader::BiffReader(std::istream &in) : m_in{&in} {}

bool BiffReader::next_record() {
  if (m_remaining > 0) {
    m_in->ignore(static_cast<std::streamsize>(m_remaining));
    m_remaining = 0;
  }

  const std::optional header = util::byte_stream::try_read<RecordHeader>(*m_in);
  if (!header) {
    return false;
  }

  m_record_type = header->type;
  m_remaining = header->size;
  return true;
}

void BiffReader::seek(const std::uint32_t offset) {
  m_in->clear();
  m_in->seekg(offset);
  m_record_type = 0;
  m_remaining = 0;
}

std::uint16_t BiffReader::record_type() const { return m_record_type; }

std::size_t BiffReader::remaining() const { return m_remaining; }

void BiffReader::next_continue() {
  if (!next_record() || m_record_type != biff_continue) {
    throw std::runtime_error("xls: expected CONTINUE record");
  }
}

std::uint8_t BiffReader::read_u8() {
  std::uint8_t value{};
  read_bytes(&value, sizeof(value));
  return value;
}

std::uint16_t BiffReader::read_u16() {
  std::uint16_t value{};
  read_bytes(&value, sizeof(value));
  return value;
}

std::uint32_t BiffReader::read_u32() {
  std::uint32_t value{};
  read_bytes(&value, sizeof(value));
  return value;
}

void BiffReader::read_bytes(void *out, const std::size_t count) {
  auto *cursor = static_cast<char *>(out);
  std::size_t left = count;
  while (left > 0) {
    if (m_remaining == 0) {
      next_continue();
    }
    const std::size_t take = std::min(left, m_remaining);
    util::byte_stream::read(*m_in, cursor, take);
    cursor += take;
    left -= take;
    m_remaining -= take;
  }
}

void BiffReader::skip_bytes(const std::size_t count) {
  std::size_t left = count;
  while (left > 0) {
    if (m_remaining == 0) {
      next_continue();
    }
    const std::size_t take = std::min(left, m_remaining);
    m_in->ignore(static_cast<std::streamsize>(take));
    left -= take;
    m_remaining -= take;
  }
}

std::string BiffReader::read_unicode_chars(const std::size_t cch,
                                           bool high_byte) {
  std::u16string buffer;
  buffer.reserve(cch);

  std::size_t left = cch;
  while (left > 0) {
    if (m_remaining == 0) {
      // Character data continued in a CONTINUE record starts with a fresh
      // flags byte re-declaring the encoding ([MS-XLS] 2.5.293).
      next_continue();
      high_byte = (read_u8() & 0x01) != 0;
    }
    const std::size_t available = high_byte ? m_remaining / 2 : m_remaining;
    if (available == 0) {
      throw std::runtime_error("xls: malformed string continuation");
    }
    const std::size_t take = std::min(left, available);
    if (high_byte) {
      const std::size_t offset = buffer.size();
      buffer.resize(offset + take);
      read_bytes(buffer.data() + offset, take * sizeof(char16_t));
    } else {
      for (std::size_t i = 0; i < take; ++i) {
        // Compressed characters are code points U+0000 to U+00FF.
        buffer.push_back(read_u8());
      }
    }
    left -= take;
  }

  return util::string::u16string_to_string(buffer);
}

std::string BiffReader::read_short_xl_unicode_string() {
  const std::uint8_t cch = read_u8();
  const std::uint8_t flags = read_u8();
  return read_unicode_chars(cch, (flags & 0x01) != 0);
}

std::string BiffReader::read_xl_unicode_string() {
  const std::uint16_t cch = read_u16();
  const std::uint8_t flags = read_u8();
  return read_unicode_chars(cch, (flags & 0x01) != 0);
}

std::string BiffReader::read_xl_unicode_rich_extended_string() {
  const std::uint16_t cch = read_u16();
  const std::uint8_t flags = read_u8();
  const bool high_byte = (flags & 0x01) != 0;
  const bool ext_st = (flags & 0x04) != 0;
  const bool rich_st = (flags & 0x08) != 0;

  const std::uint16_t c_run = rich_st ? read_u16() : 0;
  const std::uint32_t cb_ext_rst = ext_st ? read_u32() : 0;

  std::string result = read_unicode_chars(cch, high_byte);

  // Drop the formatting runs (FormatRun is 4 bytes) and the phonetic data.
  skip_bytes(static_cast<std::size_t>(c_run) * 4);
  skip_bytes(cb_ext_rst);

  return result;
}

void BiffReader::expect_bof() {
  if (!next_record() || record_type() != biff_bof) {
    throw std::runtime_error("xls: expected BOF record");
  }
  const auto bof = read<BofFixed>();
  if (bof.vers != bof_vers_biff8) {
    throw std::runtime_error("xls: unsupported BIFF version " +
                             std::to_string(bof.vers));
  }
}

} // namespace odr::internal::oldms::spreadsheet

namespace odr::internal::oldms {

double spreadsheet::decode_rk(const std::uint32_t rk) {
  const bool x100 = (rk & 0x01) != 0;
  const bool is_int = (rk & 0x02) != 0;

  double value;
  if (is_int) {
    // 30-bit signed integer in the high bits.
    value = static_cast<std::int32_t>(rk) >> 2;
  } else {
    // High 30 bits are the high 30 bits of an IEEE double, the rest is zero.
    const std::uint64_t bits = static_cast<std::uint64_t>(rk & 0xFFFFFFFC)
                               << 32;
    value = std::bit_cast<double>(bits);
  }

  return x100 ? value / 100.0 : value;
}

std::string spreadsheet::format_number(const double value) {
  if (std::isnan(value)) {
    return "NaN";
  }

  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.15g", value);
  return buffer;
}

std::string spreadsheet::error_code_string(const std::uint8_t error) {
  switch (error) {
  case 0x00:
    return "#NULL!";
  case 0x07:
    return "#DIV/0!";
  case 0x0F:
    return "#VALUE!";
  case 0x17:
    return "#REF!";
  case 0x1D:
    return "#NAME?";
  case 0x24:
    return "#NUM!";
  case 0x2A:
    return "#N/A";
  case 0x2B:
    return "#GETTING_DATA";
  default:
    return "#ERR";
  }
}

} // namespace odr::internal::oldms
