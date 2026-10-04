#pragma once

#include <odr/internal/abstract/file.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace odr::internal::cfb::impl {

using Sector = std::uint32_t;

static constexpr std::uint32_t NullId = 0xFFFFFFFF;
static constexpr std::uint32_t RootId = 0;

#pragma pack(push, 1)

struct CompoundFileHeader {
  std::array<std::uint8_t, 8> signature;
  std::array<std::uint8_t, 16> unused_clsid;
  std::uint16_t minor_version;
  std::uint16_t major_version;
  std::uint16_t byte_order;
  std::uint16_t sector_shift;
  std::uint16_t mini_sector_shift;
  std::array<std::uint8_t, 6> reserved;
  std::uint32_t num_directory_sector;
  std::uint32_t num_fat_sector;
  std::uint32_t first_directory_sector_location;
  std::uint32_t transaction_signature_number;
  std::uint32_t mini_stream_cutoff_size;
  std::uint32_t first_mini_fat_sector_location;
  std::uint32_t num_mini_fat_sector;
  std::uint32_t first_difat_sector_location;
  std::uint32_t num_difat_sector;
  std::array<std::uint32_t, 109> header_difat;
};

struct CompoundFileEntry {
  std::array<char16_t, 32> name;
  std::uint16_t name_len;
  std::uint8_t type;
  std::uint8_t color_flag;
  std::uint32_t left_sibling_id;
  std::uint32_t right_sibling_id;
  std::uint32_t child_id;
  std::array<std::uint8_t, 16> clsid;
  std::uint32_t state_bits;
  std::uint64_t creation_time;
  std::uint64_t modified_time;
  std::uint32_t start_sector_location;
  std::uint64_t size;

  [[nodiscard]] bool is_property_stream() const {
    // defined in [MS-OLEPS] 2.23 "Property Set Stream and Storage Names"
    return name[0] == 5;
  }
  [[nodiscard]] bool is_file() const { return type == 2; }
  [[nodiscard]] bool is_directory() const { return !is_file(); }
  [[nodiscard]] std::string get_name() const;
};

#pragma pack(pop)

static_assert(sizeof(CompoundFileHeader) == 512);
static_assert(sizeof(CompoundFileEntry) == 128);

void parse_header(std::istream &in, CompoundFileHeader &hdr);
void parse_entry(std::istream &in, CompoundFileEntry &entry);
CompoundFileEntry parse_entry(std::istream &in);

class CompoundFileReader final {
public:
  static constexpr auto MAGIC = "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1";

  explicit CompoundFileReader(std::istream &in, std::uint64_t file_size);

  [[nodiscard]] const CompoundFileHeader &get_file_header() const {
    return m_header;
  }

  [[nodiscard]] const CompoundFileEntry &get_root_entry() const {
    return m_root;
  }

  /// Reads a directory entry by ID; zero names the root ([MS-CFB] 2.6.1).
  [[nodiscard]] CompoundFileEntry parse_entry(std::istream &in,
                                              std::uint32_t entry_id) const;

  void parse_entry(std::istream &in, std::uint32_t entry_id,
                   CompoundFileEntry &entry) const;

  /// Reads @p len bytes at @p offset into a buffer of at least @p len bytes.
  void read_file(std::istream &in, const CompoundFileEntry &entry,
                 std::uint64_t offset, char *buffer, std::uint64_t len) const;

private:
  struct SectorOffset final {
    Sector sector;
    std::uint64_t offset;
  };

  static constexpr Sector MaxSector = 0xFFFFFFFA;

  void read_stream(std::istream &in, const SectorOffset &sector_offset,
                   char *buffer, std::uint64_t length) const;

  /// Reads a stream stored in mini sectors ([MS-CFB] 2.4).
  void read_mini_stream(std::istream &in, const SectorOffset &sector_offset,
                        char *buffer, std::uint64_t length) const;

  [[nodiscard]] Sector resolve_next_sector(std::istream &in,
                                           Sector sector) const;

  [[nodiscard]] Sector resolve_next_mini_sector(std::istream &in,
                                                Sector mini_sector) const;

  /// Get absolute address from sector and offset.
  [[nodiscard]] std::uint64_t
  sector_offset_to_address(const SectorOffset &sector_offset) const;

  [[nodiscard]] std::uint64_t
  mini_sector_offset_to_address(std::istream &in,
                                const SectorOffset &sector_offset) const;

  /// Follows the sector chain to the requested offset.
  [[nodiscard]] SectorOffset
  normalize_sector_offset(std::istream &in, SectorOffset sector_offset) const;

  [[nodiscard]] SectorOffset
  normalize_mini_sector_offset(std::istream &in,
                               SectorOffset sector_offset) const;

  [[nodiscard]] std::uint32_t
  resolve_fat_sector_location(std::istream &in,
                              std::uint32_t fat_sector_number) const;

  std::uint64_t m_file_size{};
  CompoundFileHeader m_header{};
  CompoundFileEntry m_root{};
  std::uint64_t m_sector_size{512};
  std::uint64_t m_mini_sector_size{64};
  std::uint32_t m_mini_stream_start_sector{0};
};

} // namespace odr::internal::cfb::impl
