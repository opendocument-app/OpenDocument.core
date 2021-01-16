#ifndef ODR_CFB_IMPL_H
#define ODR_CFB_IMPL_H

#include <cstdint>
#include <functional>
#include <string>

namespace odr::cfb::impl {

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
  std::uint16_t name[32];
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

  bool is_property_stream() const;
  bool is_stream() const;
};

struct PropertySetStreamHeader {
  std::uint8_t byte_order[2];
  std::uint16_t version;
  std::uint32_t system_identifier;
  std::uint8_t clsid[16];
  std::uint32_t num_property_sets;
  struct {
    char fmtid[16];
    std::uint32_t offset;
  } property_set_info[1];
};

struct PropertySetHeader {
  std::uint32_t size;
  std::uint32_t num_properties;
  struct {
    std::uint32_t id;
    std::uint32_t offset;
  } property_identifier_and_offset[1];
};

#pragma pack(pop)

using EnumFilesCallback =
    std::function<void(const CompoundFileEntry *entry,
                       const std::u16string &dir, std::uint32_t level)>;

class CompoundFileReader final {
public:
  CompoundFileReader(const void *buffer, std::size_t len);

  /// Get entry (directory or file) by its ID.
  /// Pass "0" to get the root directory entry. -- This is the start point to
  /// navigate the compound file. Use the returned object to access child
  /// entries.
  [[nodiscard]] const CompoundFileEntry *get_entry(std::size_t entry_id) const;

  [[nodiscard]] const CompoundFileEntry *get_root_entry() const;

  [[nodiscard]] const CompoundFileHeader *get_file_info() const;

  /// Get file(stream) data start with "offset".
  /// The buffer must have enough space to store "len" bytes. Typically "len" is
  /// derived by the steam length.
  void read_file(const CompoundFileEntry *entry, std::size_t offset,
                 char *buffer, std::size_t len) const;

  void enum_files(const CompoundFileEntry *entry, int max_level,
                  const EnumFilesCallback &callback) const;

private:
  // Enum entries with same level, including 'entry' itself
  void enum_nodes(const CompoundFileEntry *entry, std::int32_t current_level,
                  std::int32_t max_level, const std::u16string &dir,
                  const EnumFilesCallback &callback) const;

  void read_stream(std::size_t sector, std::size_t offset, char *buffer,
                   std::size_t len) const;

  // Same logic as "ReadStream" except that use MiniStream functions instead
  void read_mini_stream(std::size_t sector, std::size_t offset, char *buffer,
                        std::size_t len) const;

  [[nodiscard]] std::size_t get_next_sector(std::size_t sector) const;

  [[nodiscard]] std::size_t get_next_mini_sector(std::size_t mini_sector) const;

  /// Get absolute address from sector and offset.
  [[nodiscard]] const std::uint8_t *
  sector_offset_to_address(std::size_t sector, std::size_t offset) const;

  [[nodiscard]] const std::uint8_t *
  mini_sector_offset_to_address(std::size_t sector, std::size_t offset) const;

  /// Locate the final sector/offset when original offset expands multiple
  /// sectors
  void locate_final_sector(std::size_t sector, std::size_t offset,
                           std::size_t *final_sector,
                           std::size_t *final_offset) const;

  void locate_final_mini_sector(std::size_t sector, std::size_t offset,
                                std::size_t *final_sector,
                                std::size_t *final_offset) const;

  [[nodiscard]] std::size_t
  GetFATSectorLocation(std::size_t fat_sector_number) const;

private:
  const std::uint8_t *m_buffer;
  std::size_t m_buffer_len;

  const CompoundFileHeader *m_hdr;
  std::size_t m_sector_size;
  std::size_t m_mini_sector_size;
  std::size_t m_mini_stream_start_sector;
};

class PropertySet final {
public:
  PropertySet(const void *buffer, std::size_t len, const char *fmt_id);

  /// return the string property in UTF-16 format
  const std::uint16_t *get_string_property(std::uint32_t property_id);

  /// Note: Getting property of types other than "string" is not implemented
  /// yet.
  ///       However most other types are simpler than string so can be easily
  ///       added. see [MS-OLEPS]
  const char *get_fmt_id();

private:
  const std::uint8_t *m_buffer;
  std::size_t m_buffer_len;
  const PropertySetHeader *m_hdr;
  const char *m_fmtid; // 16 bytes
};

class PropertySetStream final {
public:
  PropertySetStream(const void *buffer, std::size_t len);

  std::size_t get_property_set_count();

  PropertySet get_property_set(std::size_t index);

private:
  const std::uint8_t *m_buffer;
  std::size_t m_buffer_len;
  const PropertySetStreamHeader *m_hdr;
};

} // namespace odr::cfb::impl

#endif // ODR_CFB_IMPL_H
