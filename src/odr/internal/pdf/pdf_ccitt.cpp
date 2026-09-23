#include <odr/internal/pdf/pdf_ccitt.hpp>

#include <odr/exceptions.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace odr::internal {
namespace {

struct CcittError final : Exception {
  explicit CcittError(const char *what) : Exception(what) {}
};

[[noreturn]] void fail(const char *what) { throw CcittError(what); }

/// Past this the parameters are corrupt, not a scan.
constexpr std::int64_t max_pixels = 1'000'000'000;

struct RunCode {
  std::string_view bits;
  std::uint16_t run;
};

// ITU-T T.4 Table 2.
constexpr std::array<RunCode, 64> white_terminating{{
    {"00110101", 0},  {"000111", 1},    {"0111", 2},      {"1000", 3},
    {"1011", 4},      {"1100", 5},      {"1110", 6},      {"1111", 7},
    {"10011", 8},     {"10100", 9},     {"00111", 10},    {"01000", 11},
    {"001000", 12},   {"000011", 13},   {"110100", 14},   {"110101", 15},
    {"101010", 16},   {"101011", 17},   {"0100111", 18},  {"0001100", 19},
    {"0001000", 20},  {"0010111", 21},  {"0000011", 22},  {"0000100", 23},
    {"0101000", 24},  {"0101011", 25},  {"0010011", 26},  {"0100100", 27},
    {"0011000", 28},  {"00000010", 29}, {"00000011", 30}, {"00011010", 31},
    {"00011011", 32}, {"00010010", 33}, {"00010011", 34}, {"00010100", 35},
    {"00010101", 36}, {"00010110", 37}, {"00010111", 38}, {"00101000", 39},
    {"00101001", 40}, {"00101010", 41}, {"00101011", 42}, {"00101100", 43},
    {"00101101", 44}, {"00000100", 45}, {"00000101", 46}, {"00001010", 47},
    {"00001011", 48}, {"01010010", 49}, {"01010011", 50}, {"01010100", 51},
    {"01010101", 52}, {"00100100", 53}, {"00100101", 54}, {"01011000", 55},
    {"01011001", 56}, {"01011010", 57}, {"01011011", 58}, {"01001010", 59},
    {"01001011", 60}, {"00110010", 61}, {"00110011", 62}, {"00110100", 63},
}};

constexpr std::array<RunCode, 64> black_terminating{{
    {"0000110111", 0},
    {"010", 1},
    {"11", 2},
    {"10", 3},
    {"011", 4},
    {"0011", 5},
    {"0010", 6},
    {"00011", 7},
    {"000101", 8},
    {"000100", 9},
    {"0000100", 10},
    {"0000101", 11},
    {"0000111", 12},
    {"00000100", 13},
    {"00000111", 14},
    {"000011000", 15},
    {"0000010111", 16},
    {"0000011000", 17},
    {"0000001000", 18},
    {"00001100111", 19},
    {"00001101000", 20},
    {"00001101100", 21},
    {"00000110111", 22},
    {"00000101000", 23},
    {"00000010111", 24},
    {"00000011000", 25},
    {"000011001010", 26},
    {"000011001011", 27},
    {"000011001100", 28},
    {"000011001101", 29},
    {"000001101000", 30},
    {"000001101001", 31},
    {"000001101010", 32},
    {"000001101011", 33},
    {"000011010010", 34},
    {"000011010011", 35},
    {"000011010100", 36},
    {"000011010101", 37},
    {"000011010110", 38},
    {"000011010111", 39},
    {"000001101100", 40},
    {"000001101101", 41},
    {"000011011010", 42},
    {"000011011011", 43},
    {"000001010100", 44},
    {"000001010101", 45},
    {"000001010110", 46},
    {"000001010111", 47},
    {"000001100100", 48},
    {"000001100101", 49},
    {"000001010010", 50},
    {"000001010011", 51},
    {"000000100100", 52},
    {"000000110111", 53},
    {"000000111000", 54},
    {"000000100111", 55},
    {"000000101000", 56},
    {"000001011000", 57},
    {"000001011001", 58},
    {"000000101011", 59},
    {"000000101100", 60},
    {"000001011010", 61},
    {"000001100110", 62},
    {"000001100111", 63},
}};

// ITU-T T.4 Table 3.
constexpr std::array<RunCode, 27> white_makeup{{
    {"11011", 64},       {"10010", 128},      {"010111", 192},
    {"0110111", 256},    {"00110110", 320},   {"00110111", 384},
    {"01100100", 448},   {"01100101", 512},   {"01101000", 576},
    {"01100111", 640},   {"011001100", 704},  {"011001101", 768},
    {"011010010", 832},  {"011010011", 896},  {"011010100", 960},
    {"011010101", 1024}, {"011010110", 1088}, {"011010111", 1152},
    {"011011000", 1216}, {"011011001", 1280}, {"011011010", 1344},
    {"011011011", 1408}, {"010011000", 1472}, {"010011001", 1536},
    {"010011010", 1600}, {"011000", 1664},    {"010011011", 1728},
}};

constexpr std::array<RunCode, 27> black_makeup{{
    {"0000001111", 64},      {"000011001000", 128},   {"000011001001", 192},
    {"000001011011", 256},   {"000000110011", 320},   {"000000110100", 384},
    {"000000110101", 448},   {"0000001101100", 512},  {"0000001101101", 576},
    {"0000001001010", 640},  {"0000001001011", 704},  {"0000001001100", 768},
    {"0000001001101", 832},  {"0000001110010", 896},  {"0000001110011", 960},
    {"0000001110100", 1024}, {"0000001110101", 1088}, {"0000001110110", 1152},
    {"0000001110111", 1216}, {"0000001010010", 1280}, {"0000001010011", 1344},
    {"0000001010100", 1408}, {"0000001010101", 1472}, {"0000001011010", 1536},
    {"0000001011011", 1600}, {"0000001100100", 1664}, {"0000001100101", 1728},
}};

// ITU-T T.4 Table 3, the extended make-up codes both colours share.
constexpr std::array<RunCode, 13> shared_makeup{{
    {"00000001000", 1792},
    {"00000001100", 1856},
    {"00000001101", 1920},
    {"000000010010", 1984},
    {"000000010011", 2048},
    {"000000010100", 2112},
    {"000000010101", 2176},
    {"000000010110", 2240},
    {"000000010111", 2304},
    {"000000011100", 2368},
    {"000000011101", 2432},
    {"000000011110", 2496},
    {"000000011111", 2560},
}};

struct TableEntry {
  std::uint8_t length{0}; ///< 0 for no code
  std::uint16_t run{0};
};

/// Indexed by the next `Bits` bits, `Bits` being the longest code.
template <std::uint32_t Bits> class RunTable final {
public:
  template <std::size_t... N>
  explicit RunTable(const std::array<RunCode, N> &...codes) {
    (add(codes), ...);
  }

  [[nodiscard]] const TableEntry &at(const std::uint32_t index) const {
    return m_entries[index];
  }

private:
  template <std::size_t N> void add(const std::array<RunCode, N> &codes) {
    for (const auto &[bits, run] : codes) {
      std::uint32_t code = 0;
      for (const char c : bits) {
        code = (code << 1) | (c == '1' ? 1u : 0u);
      }
      const auto length = static_cast<std::uint32_t>(bits.size());
      const std::uint32_t shift = Bits - length;
      for (std::uint32_t suffix = 0; suffix < (1u << shift); ++suffix) {
        m_entries[(code << shift) | suffix] = {
            static_cast<std::uint8_t>(length), run};
      }
    }
  }

  std::array<TableEntry, std::size_t{1} << Bits> m_entries{};
};

const RunTable<12> &white_table() {
  static const RunTable<12> table(white_terminating, white_makeup,
                                  shared_makeup);
  return table;
}

const RunTable<13> &black_table() {
  static const RunTable<13> table(black_terminating, black_makeup,
                                  shared_makeup);
  return table;
}

/// MSB-first bits; past the end they read as zero.
class BitReader final {
public:
  explicit BitReader(const std::string_view data) : m_data{data} {}

  /// The next `count` (at most 24) bits without consuming them.
  [[nodiscard]] std::uint32_t peek(const std::uint32_t count) const {
    std::uint32_t window = 0;
    const std::size_t byte = m_position / 8;
    for (std::size_t i = 0; i < 4; ++i) {
      window <<= 8;
      if (byte + i < m_data.size()) {
        window |= static_cast<std::uint8_t>(m_data[byte + i]);
      }
    }
    return (window << (m_position % 8)) >> (32 - count);
  }

  void skip(const std::size_t count) { m_position += count; }

  std::uint32_t read(const std::uint32_t count) {
    const std::uint32_t value = peek(count);
    skip(count);
    return value;
  }

  void align() { m_position = (m_position + 7) / 8 * 8; }

  [[nodiscard]] bool exhausted() const {
    return m_position >= m_data.size() * 8;
  }

  /// Nothing but zero bits left: the padding after the last row.
  [[nodiscard]] bool only_zeros_remain() const {
    if (exhausted()) {
      return true;
    }
    const std::size_t byte = m_position / 8;
    const auto mask = static_cast<std::uint8_t>(0xff >> (m_position % 8));
    if ((static_cast<std::uint8_t>(m_data[byte]) & mask) != 0) {
      return false;
    }
    return std::ranges::all_of(m_data.substr(byte + 1),
                               [](const char c) { return c == '\0'; });
  }

private:
  std::string_view m_data;
  std::size_t m_position{0};
};

/// An EOL (T.4 4.1.2) after any zero fill, consumed only if it is there. No
/// run or mode code holds eleven zeros in a row.
bool read_eol(BitReader &reader) {
  BitReader probe = reader;
  std::size_t zeros = 0;
  while (!probe.exhausted() && probe.peek(1) == 0) {
    probe.skip(1);
    ++zeros;
  }
  if (zeros < 11 || probe.exhausted()) {
    return false;
  }
  probe.skip(1);
  reader = probe;
  return true;
}

/// One run: make-up codes up to the terminating code (T.4 4.1.1).
template <std::uint32_t Bits>
std::int32_t read_run(BitReader &reader, const RunTable<Bits> &table,
                      const std::int32_t limit) {
  std::int32_t total = 0;
  while (true) {
    const TableEntry &entry = table.at(reader.peek(Bits));
    if (entry.length == 0) {
      fail("ccitt: invalid run code");
    }
    reader.skip(entry.length);
    total += entry.run;
    if (total > limit) {
      fail("ccitt: run past the end of the row");
    }
    if (entry.run < 64) {
      return total;
    }
  }
}

std::int32_t read_run(BitReader &reader, const bool black,
                      const std::int32_t limit) {
  return black ? read_run(reader, black_table(), limit)
               : read_run(reader, white_table(), limit);
}

enum class Mode { pass, horizontal, vertical };

struct ModeCode {
  Mode mode;
  std::int32_t delta{0}; ///< a1 - b1, for vertical
};

/// T.4 Table 4.
ModeCode read_mode(BitReader &reader) {
  const std::uint32_t bits = reader.peek(7);
  if (bits >> 6 == 0b1) {
    reader.skip(1);
    return {Mode::vertical, 0};
  }
  switch (bits >> 4) {
  case 0b011:
    reader.skip(3);
    return {Mode::vertical, 1};
  case 0b010:
    reader.skip(3);
    return {Mode::vertical, -1};
  case 0b001:
    reader.skip(3);
    return {Mode::horizontal};
  default:
    break;
  }
  if (bits >> 3 == 0b0001) {
    reader.skip(4);
    return {Mode::pass};
  }
  switch (bits >> 1) {
  case 0b000011:
    reader.skip(6);
    return {Mode::vertical, 2};
  case 0b000010:
    reader.skip(6);
    return {Mode::vertical, -2};
  default:
    break;
  }
  switch (bits) {
  case 0b0000011:
    reader.skip(7);
    return {Mode::vertical, 3};
  case 0b0000010:
    reader.skip(7);
    return {Mode::vertical, -3};
  default:
    // `0000001` opens an uncompressed-mode extension, `0000000` an EOL
    fail("ccitt: unexpected code in a row");
  }
}

/// A row is its changing elements, where the colour flips, starting from white.
/// A damaged row's out-of-order positions are clamped.
void push_change(std::vector<std::int32_t> &row, const std::int32_t position,
                 const std::int32_t columns) {
  const std::int32_t lower = row.empty() ? 0 : row.back();
  row.push_back(std::clamp(position, lower, columns));
}

void decode_1d_row(BitReader &reader, std::vector<std::int32_t> &coding,
                   const std::int32_t columns) {
  std::int32_t a0 = 0;
  bool black = false;
  while (a0 < columns) {
    a0 += read_run(reader, black, columns);
    push_change(coding, a0, columns);
    black = !black;
  }
}

/// T.4 4.2.1.3 / T.6 2.2. `reference` ends in at least three `columns`
/// sentinels, so `b1` and `b2` always exist.
void decode_2d_row(BitReader &reader,
                   const std::vector<std::int32_t> &reference,
                   std::vector<std::int32_t> &coding,
                   const std::int32_t columns) {
  std::int32_t a0 = -1;
  bool black = false;
  std::size_t r = 0;
  while (a0 < columns) {
    // An even index turns black, an odd one white. A vertical mode can put a0
    // left of the last b1, so the search may step back.
    while (r > 0 && reference[r - 1] > a0) {
      --r;
    }
    while (reference[r] <= a0 || (r % 2 == 1) != black) {
      ++r;
    }
    const std::int32_t b1 = reference[r];
    const std::int32_t b2 = reference[r + 1];

    const ModeCode code = read_mode(reader);
    switch (code.mode) {
    case Mode::pass:
      a0 = b2;
      break;
    case Mode::horizontal: {
      const std::int32_t a1 =
          std::max(a0, 0) + read_run(reader, black, columns);
      const std::int32_t a2 = a1 + read_run(reader, !black, columns);
      push_change(coding, a1, columns);
      push_change(coding, a2, columns);
      a0 = coding.back();
      break;
    }
    case Mode::vertical:
      push_change(coding, b1 + code.delta, columns);
      a0 = coding.back();
      black = !black;
      break;
    }
  }
}

/// Cancel changes at the same position: a zero-length run is no change, and
/// the reference line must not see one.
void drop_empty_runs(std::vector<std::int32_t> &row) {
  std::size_t size = 0;
  for (const std::int32_t position : row) {
    if (size > 0 && row[size - 1] == position) {
      --size;
    } else {
      row[size++] = position;
    }
  }
  row.resize(size);
}

void paint_row(std::string &out, const std::size_t row_start,
               const std::vector<std::int32_t> &row, const std::int32_t columns,
               const bool black_is_1) {
  for (std::size_t i = 0; i < row.size(); i += 2) {
    const std::int32_t to = i + 1 < row.size() ? row[i + 1] : columns;
    for (std::int32_t x = row[i]; x < to;) {
      char &byte = out[row_start + static_cast<std::size_t>(x / 8)];
      if (x % 8 == 0 && to - x >= 8) {
        byte = black_is_1 ? '\xff' : '\0';
        x += 8;
        continue;
      }
      const auto mask = static_cast<std::uint8_t>(0x80 >> (x % 8));
      const auto value = static_cast<std::uint8_t>(byte);
      byte = static_cast<char>(black_is_1 ? value | mask : value & ~mask);
      ++x;
    }
  }
}

} // namespace

std::optional<std::string>
pdf::decode_ccitt(const std::string_view data,
                  const CcittParameters &parameters) {
  const std::int32_t columns = parameters.columns;
  const std::int32_t rows = parameters.rows;
  if (columns <= 0 || rows < 0 ||
      static_cast<std::int64_t>(columns) * rows > max_pixels) {
    return std::nullopt;
  }
  const std::size_t row_bytes = (static_cast<std::size_t>(columns) + 7) / 8;
  const char white = parameters.black_is_1 ? '\0' : '\xff';

  BitReader reader(data);
  std::vector<std::int32_t> reference(3, columns); // an all-white row
  std::vector<std::int32_t> coding;
  std::string out;
  std::int32_t decoded = 0;

  try {
    while (rows == 0 || decoded < rows) {
      bool one_d = parameters.k == 0;
      if (parameters.k < 0) {
        if (parameters.encoded_byte_align) {
          reader.align();
        }
        if (reader.only_zeros_remain() || reader.peek(12) == 1) {
          break; // EOFB (T.6 2.4)
        }
      } else {
        // An aligned row's fill sits before its EOL, which ends on the byte.
        const bool eol = read_eol(reader);
        if (!eol && parameters.encoded_byte_align) {
          reader.align();
        }
        if (reader.only_zeros_remain()) {
          break;
        }
        if (parameters.k > 0) {
          one_d = reader.read(1) == 1;
        }
        if (eol && read_eol(reader)) {
          break; // RTC (T.4 4.1.4)
        }
      }

      coding.clear();
      try {
        if (one_d) {
          decode_1d_row(reader, coding, columns);
        } else {
          decode_2d_row(reader, reference, coding, columns);
        }
      } catch (const CcittError &) {
        if (!reader.only_zeros_remain()) {
          throw;
        }
        break; // the data stops inside a row
      }
      drop_empty_runs(coding);

      if (static_cast<std::int64_t>(decoded + 1) * columns > max_pixels) {
        return std::nullopt;
      }
      const std::size_t row_start = out.size();
      out.append(row_bytes, white);
      paint_row(out, row_start, coding, columns, parameters.black_is_1);
      ++decoded;

      std::swap(reference, coding);
      reference.insert(reference.end(), 3, columns);
    }
  } catch (const CcittError &) {
    return std::nullopt;
  }

  if (decoded == 0 && rows == 0) {
    return std::nullopt;
  }
  out.append(static_cast<std::size_t>(rows - std::min(rows, decoded)) *
                 row_bytes,
             white);
  return out;
}

} // namespace odr::internal
