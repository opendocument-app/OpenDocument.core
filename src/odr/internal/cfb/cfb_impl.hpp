#pragma once

#include <odr/internal/abstract/file.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace odr::internal::cfb::impl {

using Sector = std::uint32_t;

static constexpr std::uint32_t NullId = 0xFFFFFFFF;
static constexpr std::uint32_t RootId = 0;

#pragma pack(push, 1)

struct CompoundFileHeader {
  std::uint8_t signature[8];
  std::uint8_t unused_clsid[16];
  std::uint16_t minor_version;
  std::uint16_t major_version;
  std::uint16_t byte_order;
  std::uint16_t sector_shift;
  std::uint16_t mini_sector_shift;
  std::uint8_t reserved[6];
  std::uint32_t num_directory_sector;
  std::uint32_t num_fat_sector;
  std::uint32_t first_directory_sector_location;
  std::uint32_t transaction_signature_number;
  std::uint32_t mini_stream_cutoff_size;
  std::uint32_t first_mini_fat_sector_location;
  std::uint32_t num_mini_fat_sector;
  std::uint32_t first_difat_sector_location;
  std::uint32_t num_difat_sector;
  std::uint32_t header_difat[109];
};

struct CompoundFileEntry {
  char16_t name[32];
  std::uint16_t name_len;
  std::uint8_t type;
  std::uint8_t color_flag;
  std::uint32_t left_sibling_id;
  std::uint32_t right_sibling_id;
  std::uint32_t child_id;
  std::uint8_t clsid[16];
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

void parse_header(std::istream &in, CompoundFileHeader &hdr);
void parse_entry(std::istream &in, CompoundFileEntry &entry);
CompoundFileEntry parse_entry(std::istream &in);

class CompoundFileReader final {
public:
  using EnumFilesCallback =
      std::function<void(const CompoundFileEntry &entry,
                         const std::u16string &dir, std::uint32_t level)>;

  static constexpr auto MAGIC = "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1";

  explicit CompoundFileReader(std::istream &in, std::uint64_t file_size);

  [[nodiscard]] const CompoundFileHeader &get_file_header() const {
    return m_header;
  }

  [[nodiscard]] const CompoundFileEntry &get_root_entry() const {
    return m_root;
  }

  /// Get entry (directory or file) by its ID.
  /// Pass "0" to get the root directory entry. -- This is the start point to
  /// navigate the compound file. Use the returned object to access child
  /// entries.
  [[nodiscard]] CompoundFileEntry parse_entry(std::istream &in,
                                              std::uint32_t entry_id) const;

  void parse_entry(std::istream &in, std::uint32_t entry_id,
                   CompoundFileEntry &entry) const;

  /// Get file(stream) data start with "offset".
  /// The buffer must have enough space to store "len" bytes. Typically, "len"
  /// is derived by the steam length.
  void read_file(std::istream &in, const CompoundFileEntry &entry,
                 std::uint64_t offset, char *buffer, std::uint64_t len) const;

  void visit_descendants(std::istream &in, const CompoundFileEntry &entry,
                         int max_level,
                         const EnumFilesCallback &callback) const;

private:
  struct SectorOffset final {
    Sector sector;
    std::uint64_t offset;
  };

  static constexpr Sector MaxSector = 0xFFFFFFFA;

  // Enum entries with same level, including 'entry' itself
  void visit_descendants(std::istream &in, const CompoundFileEntry &entry,
                         std::int32_t current_level, std::int32_t max_level,
                         const std::u16string &dir,
                         const EnumFilesCallback &callback) const;

  void read_stream(std::istream &in, const SectorOffset &sector_offset,
                   char *buffer, std::uint64_t length) const;

  // Same logic as "ReadStream" except that use MiniStream functions instead
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

  /// Locate the final sector/offset when original offset expands multiple
  /// sectors
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
