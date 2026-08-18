#include <odr/internal/pdf/pdf_jbig2.hpp>

#include <odr/exceptions.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace odr::internal {
namespace {

/// Thrown for anything we cannot decode; `decode_jbig2` turns it into
/// `nullopt`.
struct Jbig2Error final : Exception {
  explicit Jbig2Error(const char *what) : Exception(what) {}
};

[[noreturn]] void fail(const char *what) { throw Jbig2Error(what); }

/// Past this a header is corrupt, not a scan; the decoder holds a byte a
/// pixel.
constexpr std::int64_t max_pixels = 200'000'000;

// --- MQ arithmetic decoder (ITU-T T.88 Annex E) ----------------------------

struct QeEntry {
  std::uint16_t qe;
  std::uint8_t nmps;
  std::uint8_t nlps;
  std::uint8_t switch_flag;
};

constexpr std::array<QeEntry, 47> qe_table{{
    {0x5601, 1, 1, 1},   {0x3401, 2, 6, 0},   {0x1801, 3, 9, 0},
    {0x0ac1, 4, 12, 0},  {0x0521, 5, 29, 0},  {0x0221, 38, 33, 0},
    {0x5601, 7, 6, 1},   {0x5401, 8, 14, 0},  {0x4801, 9, 14, 0},
    {0x3801, 10, 14, 0}, {0x3001, 11, 17, 0}, {0x2401, 12, 18, 0},
    {0x1c01, 13, 20, 0}, {0x1601, 29, 21, 0}, {0x5601, 15, 14, 1},
    {0x5401, 16, 14, 0}, {0x5101, 17, 15, 0}, {0x4801, 18, 16, 0},
    {0x3801, 19, 17, 0}, {0x3401, 20, 18, 0}, {0x3001, 21, 19, 0},
    {0x2801, 22, 19, 0}, {0x2401, 23, 20, 0}, {0x2201, 24, 21, 0},
    {0x1c01, 25, 22, 0}, {0x1801, 26, 23, 0}, {0x1601, 27, 24, 0},
    {0x1401, 28, 25, 0}, {0x1201, 29, 26, 0}, {0x1101, 30, 27, 0},
    {0x0ac1, 31, 28, 0}, {0x09c1, 32, 29, 0}, {0x08a1, 33, 30, 0},
    {0x0521, 34, 31, 0}, {0x0441, 35, 32, 0}, {0x02a1, 36, 33, 0},
    {0x0221, 37, 34, 0}, {0x0141, 38, 35, 0}, {0x0111, 39, 36, 0},
    {0x0085, 40, 37, 0}, {0x0049, 41, 38, 0}, {0x0025, 42, 39, 0},
    {0x0015, 43, 40, 0}, {0x0009, 44, 41, 0}, {0x0005, 45, 42, 0},
    {0x0001, 45, 43, 0}, {0x5601, 46, 46, 0},
}};

/// The spec's `CX` array: `(I << 1) | MPS` per context.
using ContextSet = std::vector<std::uint8_t>;

/// Software-conventions decoder (T.88 E.3.2). `C` is two 16-bit halves so the
/// shifts stay inside 32 bits.
class MqDecoder final {
public:
  MqDecoder(const std::string_view data, const std::size_t start)
      : m_data{data}, m_bp{start} {
    m_c_high = byte_at(m_bp);
    byte_in();
    m_c_high = ((m_c_high << 7) & 0xffff) | ((m_c_low >> 9) & 0x7f);
    m_c_low = (m_c_low << 7) & 0xffff;
    m_ct -= 7;
    m_a = 0x8000;
  }

  [[nodiscard]] std::uint8_t decode(ContextSet &contexts,
                                    const std::uint32_t context) {
    if (context >= contexts.size()) {
      fail("jbig2: context out of range");
    }
    std::uint8_t index = contexts[context] >> 1;
    std::uint8_t mps = contexts[context] & 1;
    const QeEntry &entry = qe_table[index];

    std::uint32_t a = m_a - entry.qe;
    std::uint8_t d = 0;
    if (m_c_high < entry.qe) {
      // LPS exchange (E.3.2, figure E.20).
      if (a < entry.qe) {
        d = mps;
        index = entry.nmps;
      } else {
        d = 1 - mps;
        if (entry.switch_flag != 0) {
          mps = d;
        }
        index = entry.nlps;
      }
      a = entry.qe;
    } else {
      m_c_high -= entry.qe;
      if ((a & 0x8000) != 0) {
        m_a = a;
        return mps;
      }
      // MPS exchange (figure E.19).
      if (a < entry.qe) {
        d = 1 - mps;
        if (entry.switch_flag != 0) {
          mps = d;
        }
        index = entry.nlps;
      } else {
        d = mps;
        index = entry.nmps;
      }
    }

    do { // RENORMD (figure E.18)
      if (m_ct == 0) {
        byte_in();
      }
      a <<= 1;
      m_c_high = ((m_c_high << 1) & 0xffff) | ((m_c_low >> 15) & 1);
      m_c_low = (m_c_low << 1) & 0xffff;
      --m_ct;
    } while ((a & 0x8000) == 0);

    m_a = a;
    contexts[context] = static_cast<std::uint8_t>((index << 1) | mps);
    return d;
  }

private:
  [[nodiscard]] std::uint32_t byte_at(const std::size_t index) const {
    return index < m_data.size()
               ? static_cast<std::uint8_t>(m_data[index])
               : 0xff; // past the end reads as the 1-bits of a marker
  }

  void byte_in() { // BYTEIN (figure E.17)
    if (byte_at(m_bp) == 0xff) {
      if (byte_at(m_bp + 1) > 0x8f) {
        m_c_low += 0xff00; // a marker: feed 1-bits and stop consuming
        m_ct = 8;
      } else {
        ++m_bp;
        m_c_low += byte_at(m_bp) << 9;
        m_ct = 7;
      }
    } else {
      ++m_bp;
      m_c_low += byte_at(m_bp) << 8;
      m_ct = 8;
    }
    if (m_c_low > 0xffff) {
      m_c_high += m_c_low >> 16;
      m_c_low &= 0xffff;
    }
  }

  std::string_view m_data;
  std::size_t m_bp{0};
  std::uint32_t m_c_high{0};
  std::uint32_t m_c_low{0};
  std::uint32_t m_a{0};
  std::int32_t m_ct{0};
};

// --- Arithmetic integer decoding (T.88 Annex A) ----------------------------

/// The `IAx` integer contexts: 512 states, indexed by the decoded prefix.
struct IntContext {
  ContextSet contexts = ContextSet(512, 0);
};

constexpr std::int32_t int_oob = std::numeric_limits<std::int32_t>::min();

/// A.2: one integer, or `int_oob` for the out-of-band value ending a run.
std::int32_t decode_int(MqDecoder &decoder, IntContext &context) {
  std::uint32_t prev = 1;
  const auto bit = [&] {
    const std::uint8_t b = decoder.decode(context.contexts, prev);
    prev = prev < 256 ? (prev << 1) | b : (((((prev << 1) | b) & 511)) | 256);
    return static_cast<std::int32_t>(b);
  };
  const auto bits = [&](const int count) {
    std::int32_t value = 0;
    for (int i = 0; i < count; ++i) {
      value = (value << 1) | bit();
    }
    return value;
  };

  const std::int32_t sign = bit();
  std::int32_t value = 0;
  if (bit() == 0) {
    value = bits(2);
  } else if (bit() == 0) {
    value = bits(4) + 4;
  } else if (bit() == 0) {
    value = bits(6) + 20;
  } else if (bit() == 0) {
    value = bits(8) + 84;
  } else if (bit() == 0) {
    value = bits(12) + 340;
  } else {
    value = bits(32) + 4436;
  }

  if (sign == 0) {
    return value;
  }
  if (value == 0) {
    return int_oob; // negative zero is OOB (A.2, step 7)
  }
  return -value;
}

/// A.3: the symbol id, `code_length` bits through its own context tree.
std::uint32_t decode_iaid(MqDecoder &decoder, ContextSet &contexts,
                          const std::uint32_t code_length) {
  std::uint32_t prev = 1;
  for (std::uint32_t i = 0; i < code_length; ++i) {
    prev = (prev << 1) | decoder.decode(contexts, prev);
  }
  return prev - (1u << code_length);
}

// --- Bitmaps ---------------------------------------------------------------

/// A byte a pixel: the generic-region templates read scattered neighbours, and
/// packing would cost more than the memory. A 1 is black.
struct Bitmap {
  std::int32_t width{0};
  std::int32_t height{0};
  std::vector<std::uint8_t> pixels;

  Bitmap() = default;
  Bitmap(const std::int32_t w, const std::int32_t h,
         const std::uint8_t fill = 0)
      : width{w}, height{h} {
    if (w < 0 || h < 0 || static_cast<std::int64_t>(w) * h > max_pixels) {
      fail("jbig2: implausible bitmap size");
    }
    pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
                  fill);
  }

  [[nodiscard]] std::uint8_t get(const std::int32_t x,
                                 const std::int32_t y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      return 0; // outside the bitmap reads as white (6.2.5.7)
    }
    return pixels[static_cast<std::size_t>(y) * width + x];
  }

  void set(const std::int32_t x, const std::int32_t y,
           const std::uint8_t value) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
      return;
    }
    pixels[static_cast<std::size_t>(y) * width + x] = value;
  }
};

/// The external combination operators (7.4.1.5 / 6.2.2).
enum class CombOp : std::uint8_t {
  or_ = 0,
  and_ = 1,
  xor_ = 2,
  xnor_ = 3,
  replace = 4
};

void compose(Bitmap &page, const Bitmap &region, const std::int32_t x0,
             const std::int32_t y0, const CombOp op) {
  for (std::int32_t y = 0; y < region.height; ++y) {
    for (std::int32_t x = 0; x < region.width; ++x) {
      const std::int32_t px = x0 + x;
      const std::int32_t py = y0 + y;
      if (px < 0 || py < 0 || px >= page.width || py >= page.height) {
        continue;
      }
      const std::uint8_t s =
          region.pixels[static_cast<std::size_t>(y) * region.width + x];
      const std::uint8_t d = page.get(px, py);
      std::uint8_t value = s;
      switch (op) {
      case CombOp::or_:
        value = d | s;
        break;
      case CombOp::and_:
        value = d & s;
        break;
      case CombOp::xor_:
        value = d ^ s;
        break;
      case CombOp::xnor_:
        value = static_cast<std::uint8_t>((d ^ s) ^ 1);
        break;
      case CombOp::replace:
        value = s;
        break;
      }
      page.set(px, py, value);
    }
  }
}

// --- Generic region decoding (6.2) -----------------------------------------

struct Point {
  std::int8_t x;
  std::int8_t y;
};

/// The fixed part of each `GBTEMPLATE`'s neighbourhood (6.2.5.3); the adaptive
/// (`AT`) pixels join it.
const std::array<std::vector<Point>, 4> coding_templates{{
    {{-1, -2},
     {0, -2},
     {1, -2},
     {-2, -1},
     {-1, -1},
     {0, -1},
     {1, -1},
     {2, -1},
     {-4, 0},
     {-3, 0},
     {-2, 0},
     {-1, 0}},
    {{-1, -2},
     {0, -2},
     {1, -2},
     {2, -2},
     {-2, -1},
     {-1, -1},
     {0, -1},
     {1, -1},
     {2, -1},
     {-3, 0},
     {-2, 0},
     {-1, 0}},
    {{-1, -2},
     {0, -2},
     {1, -2},
     {-2, -1},
     {-1, -1},
     {0, -1},
     {1, -1},
     {-2, 0},
     {-1, 0}},
    {{-3, -1},
     {-2, -1},
     {-1, -1},
     {0, -1},
     {1, -1},
     {-4, 0},
     {-3, 0},
     {-2, 0},
     {-1, 0}},
}};

/// The context the SLTP decision is coded in when `TPGDON` is set (6.2.5.7).
constexpr std::array<std::uint32_t, 4> typical_prediction_context{
    0x9b25, 0x0795, 0x00e5, 0x0195};

/// Decode a generic region bitmap (6.2.5.7), arithmetic coding only.
///
/// Template pixels are ordered by row then column, as the spec's figures and
/// `TPGDON` constants read them. A non-nominal `AT` pixel lands in a different
/// bit than the spec gives it, which is harmless: the context is only a label,
/// and the decoder adapts per context.
Bitmap decode_generic_region(const std::int32_t width,
                             const std::int32_t height,
                             const std::uint8_t template_index,
                             const std::vector<Point> &at, const bool tpgdon,
                             MqDecoder &decoder, ContextSet &contexts,
                             const Bitmap *skip) {
  if (template_index > 3) {
    fail("jbig2: unknown generic region template");
  }
  std::vector<Point> tmpl = coding_templates[template_index];
  tmpl.insert(tmpl.end(), at.begin(), at.end());
  std::ranges::stable_sort(tmpl, [](const Point &a, const Point &b) {
    return a.y != b.y ? a.y < b.y : a.x < b.x;
  });
  if (tmpl.size() > 16) {
    fail("jbig2: oversized generic region template");
  }

  Bitmap bitmap(width, height);
  bool ltp = false;
  for (std::int32_t y = 0; y < height; ++y) {
    if (tpgdon) {
      const std::uint8_t bit =
          decoder.decode(contexts, typical_prediction_context[template_index]);
      if (bit != 0) {
        ltp = !ltp;
      }
      if (ltp) {
        // A "typical" row repeats the one above it verbatim.
        if (y > 0) {
          const auto row = static_cast<std::size_t>(y) * width;
          const std::size_t above = row - width;
          std::copy_n(
              bitmap.pixels.begin() + static_cast<std::ptrdiff_t>(above), width,
              bitmap.pixels.begin() + static_cast<std::ptrdiff_t>(row));
        }
        continue;
      }
    }
    for (std::int32_t x = 0; x < width; ++x) {
      if (skip != nullptr && skip->get(x, y) != 0) {
        bitmap.set(x, y, 0);
        continue;
      }
      std::uint32_t context = 0;
      for (const Point &p : tmpl) {
        context = (context << 1) | bitmap.get(x + p.x, y + p.y);
      }
      bitmap.set(x, y, decoder.decode(contexts, context));
    }
  }
  return bitmap;
}

// --- Segment reading -------------------------------------------------------

/// Big-endian reader over a segment's bytes; every read is bounds-checked.
class Reader final {
public:
  explicit Reader(const std::string_view data) : m_data{data} {}

  [[nodiscard]] std::uint8_t u8() {
    if (m_pos >= m_data.size()) {
      fail("jbig2: truncated segment");
    }
    return static_cast<std::uint8_t>(m_data[m_pos++]);
  }
  [[nodiscard]] std::uint16_t u16() {
    const std::uint32_t high = u8();
    return static_cast<std::uint16_t>((high << 8) | u8());
  }
  [[nodiscard]] std::uint32_t u32() {
    const std::uint32_t high = u16();
    return (high << 16) | u16();
  }
  [[nodiscard]] std::int8_t i8() { return static_cast<std::int8_t>(u8()); }

  void skip(const std::size_t count) {
    if (m_data.size() - m_pos < count) {
      fail("jbig2: truncated segment");
    }
    m_pos += count;
  }

  [[nodiscard]] std::size_t position() const noexcept { return m_pos; }
  [[nodiscard]] std::string_view rest() const { return m_data.substr(m_pos); }

private:
  std::string_view m_data;
  std::size_t m_pos{0};
};

struct Segment {
  std::uint32_t number{0};
  std::uint8_t type{0};
  std::vector<std::uint32_t> referred;
  std::string_view data;
};

/// 7.2: walk the segment headers of an embedded-format stream.
std::vector<Segment> parse_segments(const std::string_view stream) {
  std::vector<Segment> segments;
  Reader reader(stream);
  while (reader.position() < stream.size()) {
    Segment segment;
    segment.number = reader.u32();
    const std::uint8_t flags = reader.u8();
    segment.type = flags & 0x3f;
    const bool page_association_4 = (flags & 0x40) != 0;

    // 7.2.4: a count of 7 in the top three bits escapes to a long form, with a
    // retain bit per referred-to segment after the 4-byte count.
    const std::uint8_t referred_flags = reader.u8();
    std::uint32_t referred_count = referred_flags >> 5;
    if (referred_count == 7) {
      Reader back(stream.substr(reader.position() - 1));
      referred_count = back.u32() & 0x1fffffff;
      reader.skip(3); // the rest of the 4-byte count
      reader.skip((referred_count + 8) / 8);
    }
    if (referred_count > stream.size()) {
      fail("jbig2: implausible referred-to segment count");
    }
    const std::size_t referred_size =
        segment.number <= 256 ? 1 : (segment.number <= 65536 ? 2 : 4);
    for (std::uint32_t i = 0; i < referred_count; ++i) {
      std::uint32_t referred = 0;
      for (std::size_t b = 0; b < referred_size; ++b) {
        referred = (referred << 8) | reader.u8();
      }
      segment.referred.push_back(referred);
    }

    reader.skip(page_association_4 ? 4 : 1);
    const std::uint32_t length = reader.u32();
    if (length == 0xffffffff) {
      fail("jbig2: segment of unknown length");
    }
    if (stream.size() - reader.position() < length) {
      fail("jbig2: segment runs past the stream");
    }
    segment.data = stream.substr(reader.position(), length);
    reader.skip(length);
    segments.push_back(std::move(segment));
  }
  return segments;
}

/// 7.4.1: the region segment information field every region carries.
struct RegionInfo {
  std::int32_t width{0};
  std::int32_t height{0};
  std::int32_t x{0};
  std::int32_t y{0};
  CombOp comb_op{CombOp::or_};
};

RegionInfo read_region_info(Reader &reader) {
  RegionInfo info;
  info.width = static_cast<std::int32_t>(reader.u32());
  info.height = static_cast<std::int32_t>(reader.u32());
  info.x = static_cast<std::int32_t>(reader.u32());
  info.y = static_cast<std::int32_t>(reader.u32());
  const std::uint8_t flags = reader.u8();
  const std::uint8_t op = flags & 0x07;
  if (op > 4) {
    fail("jbig2: unknown combination operator");
  }
  info.comb_op = static_cast<CombOp>(op);
  if (info.width <= 0 || info.height <= 0 ||
      static_cast<std::int64_t>(info.width) * info.height > max_pixels) {
    fail("jbig2: implausible region size");
  }
  return info;
}

/// The symbol code length: `ceil(log2(count))`, at least 1 (6.5.8.2.3 as
/// amended).
std::uint32_t symbol_code_length(const std::size_t count) {
  std::uint32_t bits = 1;
  while (count > (static_cast<std::size_t>(1) << bits)) {
    ++bits;
  }
  return bits;
}

using SymbolList = std::vector<std::shared_ptr<const Bitmap>>;

/// 6.5: a symbol dictionary segment, arithmetic, no refinement or aggregation.
/// Returns the input and new symbols the export run lengths select.
SymbolList decode_symbol_dictionary(const std::string_view data,
                                    const SymbolList &input) {
  Reader reader(data);
  const std::uint16_t flags = reader.u16();
  const bool huffman = (flags & 0x0001) != 0;
  const bool refinement_aggregate = (flags & 0x0002) != 0;
  // Bit 8: contexts resume a referred dictionary's retained state (7.4.3.1.1),
  // which we do not carry between segments.
  const bool context_used = (flags & 0x0100) != 0;
  const auto template_index = static_cast<std::uint8_t>((flags >> 10) & 0x03);
  if (huffman) {
    fail("jbig2: huffman symbol dictionary");
  }
  if (refinement_aggregate) {
    fail("jbig2: refinement/aggregate symbol dictionary");
  }
  if (context_used) {
    fail("jbig2: symbol dictionary reusing a retained coding context");
  }

  std::vector<Point> at;
  const std::size_t at_count = template_index == 0 ? 4 : 1;
  for (std::size_t i = 0; i < at_count; ++i) {
    const std::int8_t x = reader.i8();
    const std::int8_t y = reader.i8();
    at.push_back({x, y});
  }

  const std::uint32_t exported_count = reader.u32();
  const std::uint32_t new_count = reader.u32();
  if (new_count > 100'000 || exported_count > 100'000) {
    fail("jbig2: implausible symbol count");
  }

  MqDecoder decoder(data, reader.position());
  ContextSet generic_contexts(1 << 16, 0);
  IntContext iadh;
  IntContext iadw;
  IntContext iaex;

  SymbolList new_symbols;
  new_symbols.reserve(new_count);
  std::int32_t height = 0;
  while (new_symbols.size() < new_count) {
    const std::int32_t delta_height = decode_int(decoder, iadh);
    if (delta_height == int_oob) {
      fail("jbig2: unexpected OOB in a symbol dictionary height class");
    }
    height += delta_height;
    std::int32_t width = 0;
    while (true) {
      const std::int32_t delta_width = decode_int(decoder, iadw);
      if (delta_width == int_oob) {
        break; // end of the height class
      }
      width += delta_width;
      if (new_symbols.size() >= new_count) {
        fail("jbig2: symbol dictionary overruns its symbol count");
      }
      new_symbols.push_back(std::make_shared<const Bitmap>(
          decode_generic_region(width, height, template_index, at,
                                /*tpgdon=*/false, decoder, generic_contexts,
                                nullptr)));
    }
  }

  // 6.5.10: runs alternate not-exported/exported over input then new symbols.
  SymbolList all = input;
  all.insert(all.end(), new_symbols.begin(), new_symbols.end());
  SymbolList exported;
  std::size_t index = 0;
  bool current = false;
  while (index < all.size() && exported.size() < exported_count) {
    const std::int32_t run = decode_int(decoder, iaex);
    if (run == int_oob || run < 0) {
      fail("jbig2: bad symbol export run");
    }
    if (current) {
      for (std::int32_t i = 0; i < run && index < all.size(); ++i, ++index) {
        exported.push_back(all[index]);
      }
    } else {
      index += static_cast<std::size_t>(run);
    }
    current = !current;
  }
  return exported;
}

/// 6.4: decode a text region segment, arithmetic coding, no refinement.
Bitmap decode_text_region(const std::string_view data,
                          const SymbolList &symbols, RegionInfo &info) {
  Reader reader(data);
  info = read_region_info(reader);

  const std::uint16_t flags = reader.u16();
  const bool huffman = (flags & 0x0001) != 0;
  const bool refine = (flags & 0x0002) != 0;
  const auto log_strips = static_cast<std::uint32_t>((flags >> 2) & 0x03);
  const auto ref_corner = static_cast<std::uint8_t>((flags >> 4) & 0x03);
  const bool transposed = ((flags >> 6) & 0x01) != 0;
  const std::uint8_t comb_op = (flags >> 7) & 0x03;
  const std::uint8_t default_pixel = (flags >> 9) & 0x01;
  std::int32_t ds_offset = (flags >> 10) & 0x1f;
  if (ds_offset > 15) {
    ds_offset -= 32; // a 5-bit signed field
  }
  if (huffman) {
    fail("jbig2: huffman text region");
  }
  if (refine) {
    fail("jbig2: refined text region");
  }
  if (comb_op > 4) {
    fail("jbig2: unknown text region combination operator");
  }

  const std::int32_t strips = 1 << log_strips;
  const std::uint32_t instance_count = reader.u32();
  if (symbols.empty()) {
    fail("jbig2: text region with no symbols");
  }

  const std::uint32_t code_length = symbol_code_length(symbols.size());
  MqDecoder decoder(data, reader.position());
  IntContext iadt;
  IntContext iafs;
  IntContext iads;
  IntContext iait;
  ContextSet iaid_contexts(static_cast<std::size_t>(1) << (code_length + 1), 0);

  Bitmap region(info.width, info.height, default_pixel);

  std::int32_t strip_t = -decode_int(decoder, iadt) * strips;
  std::int32_t first_s = 0;
  std::uint32_t instances = 0;
  while (instances < instance_count) {
    const std::int32_t delta_t = decode_int(decoder, iadt);
    if (delta_t == int_oob) {
      fail("jbig2: unexpected OOB between text region strips");
    }
    strip_t += delta_t * strips;

    const std::int32_t delta_first_s = decode_int(decoder, iafs);
    if (delta_first_s == int_oob) {
      fail("jbig2: unexpected OOB at a text region strip start");
    }
    first_s += delta_first_s;
    std::int32_t cur_s = first_s;

    while (true) {
      const std::int32_t cur_t = strips == 1 ? 0 : decode_int(decoder, iait);
      if (cur_t == int_oob) {
        fail("jbig2: unexpected OOB in a text region strip offset");
      }
      const std::int32_t t = strip_t + cur_t;
      const std::uint32_t id = decode_iaid(decoder, iaid_contexts, code_length);
      if (id >= symbols.size()) {
        fail("jbig2: symbol id out of range");
      }
      const Bitmap &symbol = *symbols[id];

      // 6.4.5 3(c): the named corner decides whether the advance comes before
      // or after the blit.
      const bool right_corner = ref_corner == 2 || ref_corner == 3;
      const bool bottom_corner = ref_corner == 0 || ref_corner == 2;
      if (!transposed && right_corner) {
        cur_s += symbol.width - 1;
      } else if (transposed && bottom_corner) {
        cur_s += symbol.height - 1;
      }

      const std::int32_t s = cur_s;
      const std::int32_t x =
          transposed ? t : (right_corner ? s - symbol.width + 1 : s);
      const std::int32_t y = transposed
                                 ? (bottom_corner ? s - symbol.height + 1 : s)
                                 : (bottom_corner ? t - symbol.height + 1 : t);
      compose(region, symbol, x, y, static_cast<CombOp>(comb_op));

      if (!transposed && !right_corner) {
        cur_s += symbol.width - 1;
      } else if (transposed && !bottom_corner) {
        cur_s += symbol.height - 1;
      }

      ++instances;
      if (instances >= instance_count) {
        break;
      }
      const std::int32_t delta_s = decode_int(decoder, iads);
      if (delta_s == int_oob) {
        break; // end of the strip
      }
      cur_s += delta_s + ds_offset;
    }
  }
  return region;
}

/// 7.4.8: the page information segment's default pixel value.
std::uint8_t read_page_default_pixel(const std::string_view data) {
  Reader reader(data);
  reader.skip(16); // width, height, x and y resolution
  const std::uint8_t flags = reader.u8();
  return (flags >> 2) & 0x01;
}

/// Pack to the pipeline's sample layout, inverting: JBIG2 codes black as 1,
/// `/DeviceGray` as 0.
std::string pack_samples(const Bitmap &page) {
  const std::size_t row_bytes = (static_cast<std::size_t>(page.width) + 7) / 8;
  std::string samples(row_bytes * static_cast<std::size_t>(page.height),
                      '\xff');
  for (std::int32_t y = 0; y < page.height; ++y) {
    const std::size_t row = static_cast<std::size_t>(y) * row_bytes;
    for (std::int32_t x = 0; x < page.width; ++x) {
      if (page.pixels[static_cast<std::size_t>(y) * page.width + x] != 0) {
        samples[row + static_cast<std::size_t>(x) / 8] &=
            static_cast<char>(~(0x80 >> (x % 8)));
      }
    }
  }
  return samples;
}

pdf::Jbig2Image decode_stream(const std::string_view data,
                              const std::string_view globals) {
  std::map<std::uint32_t, SymbolList> dictionaries;
  std::unique_ptr<Bitmap> page;

  const auto symbols_for = [&](const Segment &segment) {
    SymbolList symbols;
    for (const std::uint32_t referred : segment.referred) {
      const auto it = dictionaries.find(referred);
      if (it != dictionaries.end()) {
        symbols.insert(symbols.end(), it->second.begin(), it->second.end());
      }
    }
    return symbols;
  };

  const auto run = [&](const std::string_view stream) {
    for (const Segment &segment : parse_segments(stream)) {
      switch (segment.type) {
      case 0: // symbol dictionary
        dictionaries[segment.number] =
            decode_symbol_dictionary(segment.data, symbols_for(segment));
        break;
      case 4: // intermediate text region
      case 6: // immediate text region
      case 7: // immediate lossless text region
      {
        RegionInfo info;
        const Bitmap region =
            decode_text_region(segment.data, symbols_for(segment), info);
        if (page == nullptr) {
          fail("jbig2: region before the page information segment");
        }
        compose(*page, region, info.x, info.y, info.comb_op);
        break;
      }
      case 36: // intermediate generic region
      case 38: // immediate generic region
      case 39: // immediate lossless generic region
      {
        Reader reader(segment.data);
        const RegionInfo info = read_region_info(reader);
        const std::uint8_t generic_flags = reader.u8();
        if ((generic_flags & 0x01) != 0) {
          fail("jbig2: MMR generic region");
        }
        const auto template_index =
            static_cast<std::uint8_t>((generic_flags >> 1) & 0x03);
        const bool tpgdon = ((generic_flags >> 3) & 0x01) != 0;
        std::vector<Point> at;
        for (std::size_t i = 0; i < (template_index == 0 ? 4u : 1u); ++i) {
          const std::int8_t x = reader.i8();
          const std::int8_t y = reader.i8();
          at.push_back({x, y});
        }
        MqDecoder decoder(segment.data, reader.position());
        ContextSet contexts(1 << 16, 0);
        const Bitmap region =
            decode_generic_region(info.width, info.height, template_index, at,
                                  tpgdon, decoder, contexts, nullptr);
        if (page == nullptr) {
          fail("jbig2: region before the page information segment");
        }
        compose(*page, region, info.x, info.y, info.comb_op);
        break;
      }
      case 48: { // page information
        Reader reader(segment.data);
        const auto width = static_cast<std::int32_t>(reader.u32());
        const auto height = static_cast<std::int32_t>(reader.u32());
        if (width <= 0 || height <= 0) {
          fail("jbig2: page of unknown size");
        }
        page = std::make_unique<Bitmap>(width, height,
                                        read_page_default_pixel(segment.data));
        break;
      }
      case 49: // end of page
      case 50: // end of stripe
      case 51: // end of file
      case 52: // profiles
      case 53: // tables
      case 62: // extension
        break;
      default:
        fail("jbig2: unsupported segment type");
      }
    }
  };

  if (!globals.empty()) {
    run(globals);
  }
  run(data);

  if (page == nullptr) {
    fail("jbig2: no page");
  }
  return {page->width, page->height, pack_samples(*page)};
}

} // namespace

std::optional<pdf::Jbig2Image> pdf::decode_jbig2(const std::string &data,
                                                 const std::string &globals) {
  try {
    return decode_stream(data, globals);
  } catch (const Jbig2Error &) {
    return std::nullopt;
  } catch (const std::exception &) {
    // A stream malformed enough to trip the standard library costs the image
    // too, not the page.
    return std::nullopt;
  }
}

} // namespace odr::internal
