#include <odr/internal/crypto/crypto_argon2.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <cryptopp/blake2.h>

namespace odr::internal::crypto {

namespace {

using byte = std::uint8_t;

constexpr std::uint32_t argon2_version = 0x13;
constexpr std::uint32_t argon2id_type = 2;
constexpr std::size_t block_size = 1024;
constexpr std::size_t block_words = block_size / 8;
constexpr std::uint32_t slices = 4; ///< synchronisation points per pass
constexpr std::uint32_t prehash_size = 64;
constexpr std::uint32_t max_uint32 = std::numeric_limits<std::uint32_t>::max();

/// One 1024 byte Argon2 block, as the 128 little-endian words it is mixed as.
using Block = std::array<std::uint64_t, block_words>;

std::uint64_t load64(const byte *const in) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(in[i]) << (8 * i);
  }
  return value;
}

void store32(byte *const out, const std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    out[i] = static_cast<byte>(value >> (8 * i));
  }
}

void store64(byte *const out, const std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i) {
    out[i] = static_cast<byte>(value >> (8 * i));
  }
}

const byte *bytes_of(const std::string_view in) {
  return reinterpret_cast<const byte *>(in.data());
}

void update(CryptoPP::BLAKE2b &hash, const std::string_view in) {
  if (!in.empty()) {
    hash.Update(bytes_of(in), in.size());
  }
}

void update32(CryptoPP::BLAKE2b &hash, const std::uint32_t value) {
  std::array<byte, 4> in;
  store32(in.data(), value);
  hash.Update(in.data(), in.size());
}

/// The variable-length hash H' from [RFC 9106] 3.3.
std::string blake2b_long(const std::size_t out_size,
                         const std::string_view in) {
  std::string out(out_size, '\0');
  auto *const data = reinterpret_cast<byte *>(out.data());

  if (out_size <= prehash_size) {
    CryptoPP::BLAKE2b hash(static_cast<unsigned int>(out_size));
    update32(hash, static_cast<std::uint32_t>(out_size));
    update(hash, in);
    hash.Final(data);
    return out;
  }

  // Beyond 64 bytes the output is the first half of a chain of 64 byte
  // digests, with the tail of the last one filling the remainder.
  std::array<byte, prehash_size> buffer;
  {
    CryptoPP::BLAKE2b hash(static_cast<unsigned int>(prehash_size));
    update32(hash, static_cast<std::uint32_t>(out_size));
    update(hash, in);
    hash.Final(buffer.data());
  }
  std::ranges::copy_n(buffer.begin(), 32, data);

  std::size_t written = 32;
  while (out_size - written > prehash_size) {
    CryptoPP::BLAKE2b hash(static_cast<unsigned int>(prehash_size));
    hash.Update(buffer.data(), buffer.size());
    hash.Final(buffer.data());
    std::ranges::copy_n(buffer.begin(), 32, data + written);
    written += 32;
  }

  CryptoPP::BLAKE2b hash(static_cast<unsigned int>(out_size - written));
  hash.Update(buffer.data(), buffer.size());
  hash.Final(data + written);
  return out;
}

void load_block(Block &block, const std::string_view in) {
  for (std::size_t i = 0; i < block_words; ++i) {
    block[i] = load64(bytes_of(in) + 8 * i);
  }
}

std::string store_block(const Block &block) {
  std::string out(block_size, '\0');
  auto *const data = reinterpret_cast<byte *>(out.data());
  for (std::size_t i = 0; i < block_words; ++i) {
    store64(data + 8 * i, block[i]);
  }
  return out;
}

std::uint64_t bla_mka(const std::uint64_t x, const std::uint64_t y) {
  return x + y + 2 * (x & max_uint32) * (y & max_uint32);
}

void mix(std::uint64_t &a, std::uint64_t &b, std::uint64_t &c,
         std::uint64_t &d) {
  a = bla_mka(a, b);
  d = std::rotr(d ^ a, 32);
  c = bla_mka(c, d);
  b = std::rotr(b ^ c, 24);
  a = bla_mka(a, b);
  d = std::rotr(d ^ a, 16);
  c = bla_mka(c, d);
  b = std::rotr(b ^ c, 63);
}

/// The permutation P from [RFC 9106] 3.6, over the 16 words of `block` picked
/// out by `at`.
void permute(Block &block, const std::array<std::size_t, 16> &at) {
  mix(block[at[0]], block[at[4]], block[at[8]], block[at[12]]);
  mix(block[at[1]], block[at[5]], block[at[9]], block[at[13]]);
  mix(block[at[2]], block[at[6]], block[at[10]], block[at[14]]);
  mix(block[at[3]], block[at[7]], block[at[11]], block[at[15]]);
  mix(block[at[0]], block[at[5]], block[at[10]], block[at[15]]);
  mix(block[at[1]], block[at[6]], block[at[11]], block[at[12]]);
  mix(block[at[2]], block[at[7]], block[at[8]], block[at[13]]);
  mix(block[at[3]], block[at[4]], block[at[9]], block[at[14]]);
}

/// The compression function G from [RFC 9106] 3.5. `accumulate` selects the
/// later passes, which XOR into `next` instead of overwriting it.
void compress(const Block &previous, const Block &reference, Block &next,
              const bool accumulate) {
  Block r;
  for (std::size_t i = 0; i < block_words; ++i) {
    r[i] = previous[i] ^ reference[i];
  }
  Block sum = r;
  if (accumulate) {
    for (std::size_t i = 0; i < block_words; ++i) {
      sum[i] ^= next[i];
    }
  }

  for (std::size_t row = 0; row < 8; ++row) {
    std::array<std::size_t, 16> at;
    for (std::size_t i = 0; i < at.size(); ++i) {
      at[i] = 16 * row + i;
    }
    permute(r, at);
  }
  for (std::size_t column = 0; column < 8; ++column) {
    std::array<std::size_t, 16> at;
    for (std::size_t i = 0; i < at.size(); ++i) {
      at[i] = 2 * column + 16 * (i / 2) + i % 2;
    }
    permute(r, at);
  }

  for (std::size_t i = 0; i < block_words; ++i) {
    next[i] = sum[i] ^ r[i];
  }
}

/// Refills `addresses` with the next 128 data-independent selectors.
void next_addresses(Block &addresses, Block &input) {
  constexpr Block zero{};
  ++input[6];
  compress(zero, input, addresses, false);
  compress(zero, addresses, addresses, false);
}

/// Maps a selector onto a block of the reference lane, [RFC 9106] 3.4.1.2.
std::uint32_t
reference_index(const std::uint32_t pass, const std::uint32_t slice,
                const std::uint32_t index, const std::uint32_t segment_length,
                const std::uint32_t lane_length, const bool same_lane,
                const std::uint64_t selector) {
  std::uint32_t area;
  if (pass == 0) {
    if (slice == 0) {
      area = index - 1;
    } else if (same_lane) {
      area = slice * segment_length + index - 1;
    } else {
      area = slice * segment_length - (index == 0 ? 1 : 0);
    }
  } else if (same_lane) {
    area = lane_length - segment_length + index - 1;
  } else {
    area = lane_length - segment_length - (index == 0 ? 1 : 0);
  }

  // A quadratic distribution over the area, biased towards recent blocks.
  const std::uint64_t low = selector & max_uint32;
  const std::uint64_t square = (low * low) >> 32;
  const std::uint64_t relative =
      static_cast<std::uint64_t>(area) - 1 - ((area * square) >> 32);

  const std::uint32_t start =
      (pass == 0 || slice == slices - 1) ? 0 : (slice + 1) * segment_length;
  return static_cast<std::uint32_t>((start + relative) % lane_length);
}

} // namespace

std::string argon2::id(const std::size_t tag_size,
                       const std::string_view password,
                       const std::string_view salt,
                       const std::size_t iterations, const std::size_t memory,
                       const std::size_t lanes) {
  if (tag_size < 4 || tag_size > max_uint32) {
    throw std::invalid_argument("argon2id: tag size out of range");
  }
  if (salt.size() < 8) {
    throw std::invalid_argument("argon2id: salt too short");
  }
  if (iterations < 1 || iterations > max_uint32) {
    throw std::invalid_argument("argon2id: iterations out of range");
  }
  if (lanes < 1 || lanes > 0xffffff) {
    throw std::invalid_argument("argon2id: lanes out of range");
  }
  if (memory < 8 * lanes || memory > max_uint32) {
    throw std::invalid_argument("argon2id: memory out of range");
  }

  const auto passes = static_cast<std::uint32_t>(iterations);
  const auto lane_count = static_cast<std::uint32_t>(lanes);
  const std::uint32_t segment_length =
      static_cast<std::uint32_t>(memory) / (lane_count * slices);
  const std::uint32_t lane_length = segment_length * slices;
  const std::uint32_t block_count = lane_length * lane_count;

  std::array<byte, prehash_size + 8> prehash;
  {
    CryptoPP::BLAKE2b hash(static_cast<unsigned int>(prehash_size));
    update32(hash, lane_count);
    update32(hash, static_cast<std::uint32_t>(tag_size));
    update32(hash, static_cast<std::uint32_t>(memory));
    update32(hash, passes);
    update32(hash, argon2_version);
    update32(hash, argon2id_type);
    update32(hash, static_cast<std::uint32_t>(password.size()));
    update(hash, password);
    update32(hash, static_cast<std::uint32_t>(salt.size()));
    update(hash, salt);
    update32(hash, 0); // secret
    update32(hash, 0); // associated data
    hash.Final(prehash.data());
  }

  std::vector<Block> blocks(block_count);
  for (std::uint32_t lane = 0; lane < lane_count; ++lane) {
    const std::size_t offset = static_cast<std::size_t>(lane) * lane_length;
    for (std::uint32_t i = 0; i < 2; ++i) {
      store32(prehash.data() + prehash_size, i);
      store32(prehash.data() + prehash_size + 4, lane);
      load_block(blocks[offset + i],
                 blake2b_long(block_size,
                              {reinterpret_cast<const char *>(prehash.data()),
                               prehash.size()}));
    }
  }

  Block address_input;
  Block addresses;
  for (std::uint32_t pass = 0; pass < passes; ++pass) {
    for (std::uint32_t slice = 0; slice < slices; ++slice) {
      for (std::uint32_t lane = 0; lane < lane_count; ++lane) {
        // Argon2id indexes the first half of the first pass without looking at
        // the data, and everything after it like Argon2d.
        const bool independent = pass == 0 && slice < slices / 2;
        if (independent) {
          address_input = {};
          address_input[0] = pass;
          address_input[1] = lane;
          address_input[2] = slice;
          address_input[3] = block_count;
          address_input[4] = passes;
          address_input[5] = argon2id_type;
        }

        std::uint32_t index = 0;
        if (pass == 0 && slice == 0) {
          index = 2; // the first two blocks of the lane are already filled
          if (independent) {
            next_addresses(addresses, address_input);
          }
        }

        for (; index < segment_length; ++index) {
          const std::uint32_t current =
              lane * lane_length + slice * segment_length + index;
          const std::uint32_t previous = current % lane_length == 0
                                             ? current + lane_length - 1
                                             : current - 1;

          std::uint64_t selector;
          if (independent) {
            if (index % block_words == 0) {
              next_addresses(addresses, address_input);
            }
            selector = addresses[index % block_words];
          } else {
            selector = blocks[previous][0];
          }

          // The first slice of the first pass has no other lane to point at.
          const std::uint32_t reference_lane =
              pass == 0 && slice == 0
                  ? lane
                  : static_cast<std::uint32_t>((selector >> 32) % lane_count);
          const std::uint32_t reference =
              reference_lane * lane_length +
              reference_index(pass, slice, index, segment_length, lane_length,
                              reference_lane == lane, selector);

          compress(blocks[previous], blocks[reference], blocks[current],
                   pass != 0);
        }
      }
    }
  }

  Block result = blocks[lane_length - 1];
  for (std::uint32_t lane = 1; lane < lane_count; ++lane) {
    const std::size_t offset = static_cast<std::size_t>(lane) * lane_length;
    const Block &last = blocks[offset + lane_length - 1];
    for (std::size_t i = 0; i < block_words; ++i) {
      result[i] ^= last[i];
    }
  }
  return blake2b_long(tag_size, store_block(result));
}

} // namespace odr::internal::crypto
