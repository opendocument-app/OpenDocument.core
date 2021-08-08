#include <algorithm>
#include <cstring>
#include <internal/cfb/cfb_impl.h>
#include <odr/exceptions.h>
#include <utility>

namespace odr::internal::cfb::impl {

namespace {
constexpr auto MAGIC = "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1";
constexpr std::size_t MAX_REG_SECT = 0xFFFFFFFA;

std::uint32_t parse_uint32(const void *buffer) {
  return *static_cast<const std::uint32_t *>(buffer);
}
} // namespace

bool CompoundFileEntry::is_property_stream() const {
  // defined in [MS-OLEPS] 2.23 "Property Set Stream and Storage Names"
  return name[0] == 5;
}

bool CompoundFileEntry::is_stream() const { return type == 2; }

CompoundFileReader::CompoundFileReader(const void *buffer,
                                       const std::size_t len)
    : m_buffer{static_cast<const std::uint8_t *>(buffer)},
      m_buffer_len{len}, m_hdr{static_cast<const CompoundFileHeader *>(buffer)},
      m_sector_size{512}, m_mini_sector_size{64}, m_mini_stream_start_sector{
                                                      0} {
  if (buffer == nullptr || len == 0) {
    throw std::invalid_argument("");
  }

  if (m_buffer_len < sizeof(*m_hdr) ||
      std::memcmp(m_hdr->signature, MAGIC, 8) != 0) {
    throw NoCfbFile();
  }

  m_sector_size = m_hdr->major_version == 3 ? 512 : 4096;

  // The file must contains at least 3 sectors
  if (m_buffer_len < m_sector_size * 3) {
    throw CfbFileCorrupted();
  }

  const CompoundFileEntry *root = get_entry(0);
  if (root == nullptr) {
    throw CfbFileCorrupted();
  }

  m_mini_stream_start_sector = root->start_sector_location;
}

const CompoundFileEntry *CompoundFileReader::get_entry(size_t entry_id) const {
  if (entry_id == 0xFFFFFFFF) {
    return nullptr;
  }

  if (m_buffer_len / sizeof(CompoundFileEntry) <= entry_id) {
    throw std::invalid_argument("");
  }

  std::size_t sector = 0;
  std::size_t offset = 0;
  locate_final_sector(m_hdr->first_directory_sector_location,
                      entry_id * sizeof(CompoundFileEntry), &sector, &offset);
  return reinterpret_cast<const CompoundFileEntry *>(
      sector_offset_to_address(sector, offset));
}

const CompoundFileEntry *CompoundFileReader::get_root_entry() const {
  return get_entry(0);
}

const CompoundFileHeader *CompoundFileReader::get_file_info() const {
  return m_hdr;
}

void CompoundFileReader::read_file(const CompoundFileEntry *entry,
                                   const std::size_t offset, char *buffer,
                                   const std::size_t len) const {
  if (entry->size < offset || entry->size - offset < len) {
    throw std::invalid_argument("");
  }

  if (entry->size < m_hdr->mini_stream_cutoff_size) {
    read_mini_stream(entry->start_sector_location, offset, buffer, len);
  } else {
    read_stream(entry->start_sector_location, offset, buffer, len);
  }
}

void CompoundFileReader::enum_files(const CompoundFileEntry *entry,
                                    int max_level,
                                    const EnumFilesCallback &callback) const {
  std::u16string dir;
  enum_nodes(get_entry(entry->child_id), 0, max_level, dir, callback);
}

void CompoundFileReader::enum_nodes(const CompoundFileEntry *entry,
                                    const std::int32_t current_level,
                                    const std::int32_t max_level,
                                    const std::u16string &dir,
                                    const EnumFilesCallback &callback) const {
  if (max_level > 0 && current_level >= max_level) {
    return;
  }
  if (entry == nullptr) {
    return;
  }

  callback(entry, dir, current_level + 1);

  const CompoundFileEntry *child = get_entry(entry->child_id);
  if (child != nullptr) {
    std::u16string new_dir = dir;
    if (dir.length() != 0) {
      new_dir.push_back('/');
    }
    new_dir.append(reinterpret_cast<const char16_t *>(entry->name),
                   entry->name_len / 2);
    enum_nodes(get_entry(entry->child_id), current_level + 1, max_level,
               new_dir, callback);
  }

  enum_nodes(get_entry(entry->left_sibling_id), current_level, max_level, dir,
             callback);
  enum_nodes(get_entry(entry->right_sibling_id), current_level, max_level, dir,
             callback);
}

void CompoundFileReader::read_stream(std::size_t sector, std::size_t offset,
                                     char *buffer, std::size_t len) const {
  locate_final_sector(sector, offset, &sector, &offset);

  // copy as many as possible in each step
  // copylen typically iterate as: m_sectorSize - offset   -->   m_sectorSize
  // -->   m_sectorSize  --> ... -->    remaining
  while (len > 0) {
    const std::uint8_t *src = sector_offset_to_address(sector, offset);
    std::size_t copylen = std::min(len, m_sector_size - offset);
    if (m_buffer + m_buffer_len < src + copylen) {
      throw CfbFileCorrupted();
    }

    std::memcpy(buffer, src, copylen);
    buffer += copylen;
    len -= copylen;
    sector = get_next_sector(sector);
    offset = 0;
  }
}

void CompoundFileReader::read_mini_stream(std::size_t sector,
                                          std::size_t offset, char *buffer,
                                          std::size_t len) const {
  locate_final_mini_sector(sector, offset, &sector, &offset);

  // copy as many as possible in each step
  // copylen typically iterate as: m_sectorSize - offset   -->   m_sectorSize
  // -->   m_sectorSize  --> ... -->    remaining
  while (len > 0) {
    const std::uint8_t *src = mini_sector_offset_to_address(sector, offset);
    std::size_t copylen = std::min(len, m_mini_sector_size - offset);
    if (m_buffer + m_buffer_len < src + copylen) {
      throw CfbFileCorrupted();
    }

    std::memcpy(buffer, src, copylen);
    buffer += copylen;
    len -= copylen;
    sector = get_next_mini_sector(sector);
    offset = 0;
  }
}

std::size_t CompoundFileReader::get_next_sector(size_t sector) const {
  // lookup FAT
  std::size_t entriesPerSector = m_sector_size / 4;
  std::size_t fatSectorNumber = sector / entriesPerSector;
  std::size_t fatSectorLocation = get_fat_sector_location(fatSectorNumber);
  return parse_uint32(sector_offset_to_address(fatSectorLocation,
                                               sector % entriesPerSector * 4));
}

std::size_t CompoundFileReader::get_next_mini_sector(size_t mini_sector) const {
  std::size_t sector, offset;
  locate_final_sector(m_hdr->first_mini_fat_sector_location, mini_sector * 4,
                      &sector, &offset);
  return parse_uint32(sector_offset_to_address(sector, offset));
}

const std::uint8_t *
CompoundFileReader::sector_offset_to_address(size_t sector,
                                             size_t offset) const {
  if (sector >= MAX_REG_SECT || offset >= m_sector_size ||
      m_buffer_len <= static_cast<std::uint64_t>(m_sector_size) * sector +
                          m_sector_size + offset) {
    throw CfbFileCorrupted();
  }

  return m_buffer + m_sector_size + m_sector_size * sector + offset;
}

const std::uint8_t *
CompoundFileReader::mini_sector_offset_to_address(std::size_t sector,
                                                  std::size_t offset) const {
  if (sector >= MAX_REG_SECT || offset >= m_mini_sector_size ||
      m_buffer_len <=
          static_cast<std::uint64_t>(m_mini_sector_size) * sector + offset) {
    throw CfbFileCorrupted();
  }

  locate_final_sector(m_mini_stream_start_sector,
                      sector * m_mini_sector_size + offset, &sector, &offset);
  return sector_offset_to_address(sector, offset);
}

void CompoundFileReader::locate_final_sector(std::size_t sector,
                                             std::size_t offset,
                                             std::size_t *final_sector,
                                             std::size_t *final_offset) const {
  while (offset >= m_sector_size) {
    offset -= m_sector_size;
    sector = get_next_sector(sector);
  }
  *final_sector = sector;
  *final_offset = offset;
}

void CompoundFileReader::locate_final_mini_sector(
    std::size_t sector, std::size_t offset, std::size_t *final_sector,
    std::size_t *final_offset) const {
  while (offset >= m_mini_sector_size) {
    offset -= m_mini_sector_size;
    sector = get_next_mini_sector(sector);
  }
  *final_sector = sector;
  *final_offset = offset;
}

std::size_t CompoundFileReader::get_fat_sector_location(
    std::size_t fat_sector_number) const {
  if (fat_sector_number < 109) {
    return m_hdr->header_difat[fat_sector_number];
  }

  fat_sector_number -= 109;
  std::size_t entriesPerSector = m_sector_size / 4 - 1;
  std::size_t difatSectorLocation = m_hdr->first_difat_sector_location;
  while (fat_sector_number >= entriesPerSector) {
    fat_sector_number -= entriesPerSector;
    const std::uint8_t *addr =
        sector_offset_to_address(difatSectorLocation, m_sector_size - 4);
    difatSectorLocation = parse_uint32(addr);
  }
  return parse_uint32(
      sector_offset_to_address(difatSectorLocation, fat_sector_number * 4));
}

PropertySet::PropertySet(const void *buffer, const std::size_t len,
                         const char *fmt_id)
    : m_buffer{static_cast<const std::uint8_t *>(buffer)}, m_buffer_len{len},
      m_hdr{static_cast<const PropertySetHeader *>(buffer)}, m_fmtid{fmt_id} {
  if (m_buffer_len < sizeof(*m_hdr) ||
      m_buffer_len < sizeof(*m_hdr) +
                         (m_hdr->num_properties - 1) *
                             sizeof(m_hdr->property_identifier_and_offset[0])) {
    throw CfbFileCorrupted();
  }
}

const std::uint16_t *PropertySet::get_string_property(uint32_t property_id) {
  for (std::uint32_t i = 0; i < m_hdr->num_properties; i++) {
    if (m_hdr->property_identifier_and_offset[i].id == property_id) {
      std::uint32_t offset = m_hdr->property_identifier_and_offset[i].offset;
      if (m_buffer_len < offset + 8) {
        throw CfbFileCorrupted();
      }
      std::uint32_t stringLengthInChar = parse_uint32(m_buffer + offset + 4);
      if (m_buffer_len < offset + 8 + stringLengthInChar * 2) {
        throw CfbFileCorrupted();
      }
      return reinterpret_cast<const std::uint16_t *>(m_buffer + offset + 8);
    }
  }

  return nullptr;
}

const char *PropertySet::get_fmt_id() { return m_fmtid; }

PropertySetStream::PropertySetStream(const void *buffer, const std::size_t len)
    : m_buffer{static_cast<const std::uint8_t *>(buffer)}, m_buffer_len{len},
      m_hdr{static_cast<const PropertySetStreamHeader *>(buffer)} {
  if (m_buffer_len < sizeof(*m_hdr) ||
      m_buffer_len < sizeof(*m_hdr) + (m_hdr->num_property_sets - 1) *
                                          sizeof(m_hdr->property_set_info[0])) {
    throw CfbFileCorrupted();
  }
}

std::size_t PropertySetStream::get_property_set_count() {
  return m_hdr->num_property_sets;
}

PropertySet PropertySetStream::get_property_set(size_t index) {
  if (index >= get_property_set_count()) {
    throw CfbFileCorrupted();
  }
  std::uint32_t offset = m_hdr->property_set_info[index].offset;
  if (m_buffer_len < offset + 4) {
    throw CfbFileCorrupted();
  }
  std::uint32_t size = parse_uint32(m_buffer + offset);
  if (m_buffer_len < offset + size) {
    throw CfbFileCorrupted();
  }
  return {m_buffer + offset, size, m_hdr->property_set_info[index].fmtid};
}

} // namespace odr::internal::cfb::impl
