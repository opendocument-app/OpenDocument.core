#include <odr/internal/pdf/pdf_cid.hpp>

#include <odr/internal/pdf/pdf_cid_data.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace odr::internal::pdf {

namespace {

// --- predefined Unicode CMaps (Uni*): the codes already are Unicode ---------

/// The Adobe predefined Unicode CMaps name their flavour in the middle segment:
/// `Uni<Collection>-UCS2-H`, `-UTF16-H`, `-UTF32-H` (optionally with a
/// `HW`/`Pro` or writing-mode suffix). UCS-2 and UTF-16 are both 2-byte
/// big-endian code units (UTF-16 additionally pairs surrogates); UTF-32 is
/// 4-byte big-endian.
enum class UnicodeCodec { utf16be, utf32be };

std::optional<UnicodeCodec> classify(const std::string_view name) {
  if (name.find("UTF32") != std::string_view::npos) {
    return UnicodeCodec::utf32be;
  }
  if (name.find("UTF16") != std::string_view::npos ||
      name.find("UCS2") != std::string_view::npos) {
    return UnicodeCodec::utf16be;
  }
  return std::nullopt;
}

std::uint8_t byte(const std::string &codes, const std::size_t i) {
  return static_cast<std::uint8_t>(codes[i]);
}

std::string decode_utf16be(const std::string &codes) {
  std::u16string units;
  units.reserve(codes.size() / 2);
  for (std::size_t i = 0; i + 1 < codes.size(); i += 2) {
    units.push_back(
        static_cast<char16_t>((byte(codes, i) << 8) | byte(codes, i + 1)));
  }
  return util::string::u16string_to_string(units);
}

std::string decode_utf32be(const std::string &codes) {
  std::string result;
  for (std::size_t i = 0; i + 3 < codes.size(); i += 4) {
    const auto codepoint = static_cast<char32_t>(
        (byte(codes, i) << 24) | (byte(codes, i + 1) << 16) |
        (byte(codes, i + 2) << 8) | byte(codes, i + 3));
    util::string::append_c32(codepoint, result);
  }
  return result;
}

// --- legacy CJK CMaps: code -> CID -> Unicode via the generated tables -------

const cid_data::PredefinedCMap *
find_predefined_cmap(const std::string_view name) {
  const auto *const begin = cid_data::predefined_cmaps.data();
  const auto *const end = begin + cid_data::predefined_cmaps.size();
  const auto *const it = std::lower_bound(
      begin, end, name,
      [](const cid_data::PredefinedCMap &cmap, const std::string_view target) {
        return cmap.name < target;
      });
  return it != end && it->name == name ? it : nullptr;
}

/// Byte width of the code at the front of `bytes`, matched *per byte* against
/// the codespace ranges (ISO 32000-1 9.7.6.2). Full-byte matching (not just the
/// leading byte) is required where ranges share a leading byte but diverge
/// later: GB18030's 2-byte (`0x8140`-`0xfefe`) and 4-byte
/// (`0x81308130`-`0xfe39fe39`) ranges both lead with `0x81`-`0xfe`, and the
/// second byte (`0x40`-`0xfe` vs `0x30`-`0x39`) decides. Ranges are stored
/// width-ascending, so the first full match is the shortest valid code. Falls
/// back to a single byte when none matches (or the tail is a partial code).
std::size_t code_width(const cid_data::PredefinedCMap &cmap,
                       const std::string_view bytes) {
  for (std::uint32_t i = 0; i < cmap.codespace_count; ++i) {
    const cid_data::CodespaceRange &range = cmap.codespace[i];
    if (bytes.size() < range.width) {
      continue;
    }
    bool matched = true;
    for (unsigned b = 0; b < range.width; ++b) {
      const auto shift = static_cast<unsigned>(8 * (range.width - 1 - b));
      const auto low = static_cast<std::uint8_t>(range.low >> shift);
      const auto high = static_cast<std::uint8_t>(range.high >> shift);
      const auto value = static_cast<std::uint8_t>(bytes[b]);
      if (value < low || value > high) {
        matched = false;
        break;
      }
    }
    if (matched) {
      return range.width;
    }
  }
  return 1;
}

/// The CID a `width`-byte code with numeric value `value` selects, via the
/// CMap's interned range index list (sorted by `(width, code_low)`); `nullopt`
/// for an unmapped code.
std::optional<std::uint32_t> code_to_cid(const cid_data::PredefinedCMap &cmap,
                                         const std::uint32_t value,
                                         const std::uint8_t width) {
  const std::pair key{width, value};
  std::uint32_t low = 0;
  std::uint32_t high = cmap.range_count;
  while (low < high) {
    const std::uint32_t mid = low + (high - low) / 2;
    const cid_data::CidRange &range =
        cid_data::cid_range_pool[cmap.ranges[mid]];
    if (std::pair{range.width, range.code_low} <= key) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  if (low == 0) {
    return std::nullopt;
  }
  const cid_data::CidRange &range =
      cid_data::cid_range_pool[cmap.ranges[low - 1]];
  if (range.width == width && value >= range.code_low &&
      value <= range.code_low + range.run_len) {
    return static_cast<std::uint32_t>(range.cid_low) + (value - range.code_low);
  }
  return std::nullopt;
}

const cid_data::Collection *find_collection(const std::string_view registry,
                                            const std::string_view ordering) {
  for (const cid_data::Collection &collection : cid_data::collections) {
    if (collection.registry == registry && collection.ordering == ordering) {
      return &collection;
    }
  }
  return nullptr;
}

/// CID -> Unicode within a collection: the presence bitmap gates the CID, then
/// `rank(cid)` (words before + popcount within the word) indexes the dense
/// value array; a `0xffff` sentinel escapes to the astral side table.
std::optional<char32_t>
collection_cid_to_unicode(const cid_data::Collection &collection,
                          const std::uint32_t cid) {
  if (cid > collection.max_cid) {
    return std::nullopt;
  }
  const std::uint32_t word = cid >> 6;
  const std::uint64_t bit = std::uint64_t{1} << (cid & 63);
  if ((collection.bitmap[word] & bit) == 0) {
    return std::nullopt;
  }
  const auto index = collection.rank[word] +
                     static_cast<std::uint32_t>(
                         std::popcount(collection.bitmap[word] & (bit - 1)));
  const std::uint16_t value = collection.values[index];
  if (value != 0xffff) {
    return static_cast<char32_t>(value);
  }
  const auto *const begin = collection.astral;
  const auto *const end = begin + collection.astral_count;
  const auto *const it = std::lower_bound(
      begin, end, index,
      [](const cid_data::AstralEntry &entry, const std::uint32_t target) {
        return entry.value_index < target;
      });
  return it != end && it->value_index == index ? std::optional{it->codepoint}
                                               : std::nullopt;
}

/// Split a code string through a named legacy CMap and translate each code
/// code -> CID -> Unicode; unmapped codes contribute nothing.
std::string translate_legacy_cmap(const cid_data::PredefinedCMap &cmap,
                                  const std::string &codes) {
  const cid_data::Collection &collection =
      cid_data::collections[cmap.collection];
  std::string result;
  std::size_t pos = 0;
  while (pos < codes.size()) {
    const std::size_t width =
        code_width(cmap, std::string_view(codes).substr(pos));
    std::uint32_t value = 0;
    for (std::size_t k = 0; k < width; ++k) {
      value = (value << 8) | byte(codes, pos + k);
    }
    pos += width;

    if (const std::optional<std::uint32_t> cid =
            code_to_cid(cmap, value, static_cast<std::uint8_t>(width));
        cid.has_value()) {
      if (const std::optional<char32_t> unicode =
              collection_cid_to_unicode(collection, *cid);
          unicode.has_value()) {
        util::string::append_c32(*unicode, result);
      }
    }
  }
  return result;
}

} // namespace

} // namespace odr::internal::pdf

namespace odr::internal {

std::optional<std::string>
pdf::translate_predefined_cmap(const std::string_view name,
                               const std::string &codes) {
  if (const std::optional<UnicodeCodec> codec = classify(name);
      codec.has_value()) {
    return *codec == UnicodeCodec::utf32be ? decode_utf32be(codes)
                                           : decode_utf16be(codes);
  }
  if (const cid_data::PredefinedCMap *const cmap = find_predefined_cmap(name);
      cmap != nullptr) {
    return translate_legacy_cmap(*cmap, codes);
  }
  return std::nullopt;
}

std::optional<char32_t> pdf::cid_to_unicode(const std::string_view registry,
                                            const std::string_view ordering,
                                            const std::uint32_t cid) {
  if (const cid_data::Collection *const collection =
          find_collection(registry, ordering);
      collection != nullptr) {
    return collection_cid_to_unicode(*collection, cid);
  }
  return std::nullopt;
}

} // namespace odr::internal
