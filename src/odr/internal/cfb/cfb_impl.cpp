#include <odr/internal/cfb/cfb_impl.hpp>

#include <odr/exceptions.hpp>
#include <odr/internal/util/byte_stream_util.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace odr::internal::cfb {

namespace {

std::uint32_t parse_uint32(std::istream &in) {
  return util::byte_stream::read<std::uint32_t>(in);
}

} // namespace

void impl::parse_header(std::istream &in, CompoundFileHeader &hdr) {
  util::byte_stream::read(in, hdr);
  // TODO deal with endianess
}

void impl::parse_entry(std::istream &in, CompoundFileEntry &entry) {
  util::byte_stream::read(in, entry);
  // TODO deal with endianess
}

impl::CompoundFileEntry impl::parse_entry(std::istream &in) {
  CompoundFileEntry entry{};
  parse_entry(in, entry);
  return entry;
}

} // namespace odr::internal::cfb

namespace odr::internal::cfb::impl {

CompoundFileReader::CompoundFileReader(std::istream &in,
                                       const std::uint64_t file_size)
    : m_file_size{file_size} {
  parse_header(in, m_header);

  if (std::memcmp(m_header.signature, MAGIC, 8) != 0) {
    throw NoCfbFile();
  }

  m_sector_size = m_header.major_version == 3 ? 512 : 4096;

  // The file must contain at least 3 sectors
  if (m_file_size < m_sector_size * 3) {
    throw CfbFileCorrupted();
  }

  impl::parse_entry(in, m_root);

  m_mini_stream_start_sector = m_root.start_sector_location;
}

CompoundFileEntry
CompoundFileReader::parse_entry(std::istream &in,
                                const std::uint32_t entry_id) const {
  const std::uint32_t offset = entry_id * sizeof(CompoundFileEntry);

  if (offset >= m_file_size) {
    throw std::invalid_argument("");
  }

  const SectorOffset sector_offset = normalize_sector_offset(
      in, {m_header.first_directory_sector_location, offset});
  const std::uint32_t address = sector_offset_to_address(sector_offset);
  in.seekg(address);
  return impl::parse_entry(in);
}

void CompoundFileReader::read_file(std::istream &in,
                                   const CompoundFileEntry &entry,
                                   const std::uint32_t offset, char *buffer,
                                   const std::uint32_t len) const {
  if (offset > entry.size) {
    throw std::invalid_argument(
        "offset bigger than entry size: " + std::to_string(offset) + " > " +
        std::to_string(entry.size));
  }
  if (len > entry.size - offset) {
    throw std::invalid_argument(
        "length bigger than remaining entry size: " + std::to_string(len) +
        " > " + std::to_string(entry.size - offset));
  }

  if (entry.size < m_header.mini_stream_cutoff_size) {
    read_mini_stream(in, {entry.start_sector_location, offset}, buffer, len);
  } else {
    read_stream(in, {entry.start_sector_location, offset}, buffer, len);
  }
}

void CompoundFileReader::visit_descendants(
    std::istream &in, const CompoundFileEntry &entry,
    const std::int32_t max_level, const EnumFilesCallback &callback) const {
  const CompoundFileEntry child_entry = parse_entry(in, entry.child_id);
  constexpr std::u16string dir;
  visit_descendants(in, child_entry, 0, max_level, dir, callback);
}

void CompoundFileReader::visit_descendants(
    std::istream &in, const CompoundFileEntry &entry,
    const std::int32_t current_level, const std::int32_t max_level,
    const std::u16string &dir, const EnumFilesCallback &callback) const {
  if (max_level > 0 && current_level >= max_level) {
    return;
  }

  callback(entry, dir, current_level + 1);

  if (entry.child_id != NullId) {
    const CompoundFileEntry child = parse_entry(in, entry.child_id);

    std::u16string new_dir = dir;
    if (!dir.empty()) {
      new_dir.push_back('/');
    }
    new_dir.append(reinterpret_cast<const char16_t *>(entry.name),
                   entry.name_len / 2);
    visit_descendants(in, child, current_level + 1, max_level, new_dir,
                      callback);
  }

  if (entry.left_sibling_id != NullId) {
    const CompoundFileEntry left_sibling =
        parse_entry(in, entry.left_sibling_id);
    visit_descendants(in, left_sibling, current_level, max_level, dir,
                      callback);
  }

  if (entry.right_sibling_id != NullId) {
    const CompoundFileEntry right_sibling =
        parse_entry(in, entry.right_sibling_id);
    visit_descendants(in, right_sibling, current_level, max_level, dir,
                      callback);
  }
}

void CompoundFileReader::read_stream(std::istream &in,
                                     const SectorOffset &sector_offset,
                                     char *buffer, std::uint32_t len) const {
  SectorOffset current_sector_offset =
      normalize_sector_offset(in, sector_offset);

  // copy as many as possible in each step
  // copy_length typically iterate as: m_sectorSize - offset   --> m_sectorSize
  // -->   m_sectorSize  --> ... -->    remaining
  while (len > 0) {
    const std::uint32_t address =
        sector_offset_to_address(current_sector_offset);
    const std::size_t copy_length =
        std::min(len, m_sector_size - current_sector_offset.offset);
    if (address + copy_length > m_file_size) {
      throw CfbFileCorrupted();
    }

    in.seekg(address);
    in.read(buffer, static_cast<std::streamsize>(copy_length));
    buffer += copy_length;
    len -= copy_length;

    current_sector_offset.sector =
        resolve_next_sector(in, current_sector_offset.sector);
    current_sector_offset.offset = 0;
  }
}

void CompoundFileReader::read_mini_stream(std::istream &in,
                                          const SectorOffset &sector_offset,
                                          char *buffer,
                                          std::uint32_t len) const {
  SectorOffset current_sector_offset =
      normalize_mini_sector_offset(in, sector_offset);

  // copy as many as possible in each step
  // copy_length typically iterate as: m_sectorSize - offset   --> m_sectorSize
  // -->   m_sectorSize  --> ... -->    remaining
  while (len > 0) {
    const std::uint32_t address =
        mini_sector_offset_to_address(in, current_sector_offset);
    const std::size_t copy_length =
        std::min(len, m_mini_sector_size - current_sector_offset.offset);
    if (address + copy_length > m_file_size) {
      throw CfbFileCorrupted();
    }

    in.seekg(address);
    in.read(buffer, static_cast<std::streamsize>(copy_length));
    buffer += copy_length;
    len -= copy_length;

    current_sector_offset.sector =
        resolve_next_mini_sector(in, current_sector_offset.sector);
    current_sector_offset.offset = 0;
  }
}

std::uint32_t
CompoundFileReader::resolve_next_sector(std::istream &in,
                                        const std::uint32_t sector) const {
  // lookup FAT
  const std::uint32_t entriesPerSector = m_sector_size / 4;
  const std::uint32_t fatSectorNumber = sector / entriesPerSector;
  const std::uint32_t fatSectorLocation =
      resolve_fat_sector_location(in, fatSectorNumber);
  const std::uint32_t address = sector_offset_to_address(
      {fatSectorLocation, sector % entriesPerSector * 4});
  in.seekg(address);
  return parse_uint32(in);
}

std::uint32_t CompoundFileReader::resolve_next_mini_sector(
    std::istream &in, const std::uint32_t mini_sector) const {
  const SectorOffset sector_offset = normalize_sector_offset(
      in, {m_header.first_mini_fat_sector_location, mini_sector * 4});
  const std::uint32_t address = sector_offset_to_address(sector_offset);
  in.seekg(address);
  return parse_uint32(in);
}

std::uint32_t CompoundFileReader::sector_offset_to_address(
    const SectorOffset &sector_offset) const {
  const std::uint32_t address = sector_offset.offset + m_sector_size +
                                sector_offset.sector * m_sector_size;

  if (sector_offset.sector >= MAX_REG_SECT ||
      sector_offset.offset >= m_sector_size || address >= m_file_size) {
    throw CfbFileCorrupted();
  }

  return address;
}

std::uint32_t CompoundFileReader::mini_sector_offset_to_address(
    std::istream &in, const SectorOffset &sector_offset) const {
  const std::uint32_t address =
      sector_offset.offset + sector_offset.sector * m_mini_sector_size;

  if (sector_offset.sector >= MAX_REG_SECT ||
      sector_offset.offset >= m_mini_sector_size || address >= m_file_size) {
    throw CfbFileCorrupted();
  }

  return sector_offset_to_address(
      normalize_sector_offset(in, {m_mini_stream_start_sector, address}));
}

CompoundFileReader::SectorOffset
CompoundFileReader::normalize_sector_offset(std::istream &in,
                                            SectorOffset sector_offset) const {
  while (sector_offset.offset >= m_sector_size) {
    sector_offset.offset -= m_sector_size;
    sector_offset.sector = resolve_next_sector(in, sector_offset.sector);
  }
  return sector_offset;
}

CompoundFileReader::SectorOffset
CompoundFileReader::normalize_mini_sector_offset(
    std::istream &in, SectorOffset sector_offset) const {
  while (sector_offset.offset >= m_mini_sector_size) {
    sector_offset.offset -= m_mini_sector_size;
    sector_offset.sector = resolve_next_mini_sector(in, sector_offset.sector);
  }
  return sector_offset;
}

std::uint32_t CompoundFileReader::resolve_fat_sector_location(
    std::istream &in, std::uint32_t fat_sector_number) const {
  if (fat_sector_number < 109) {
    return m_header.header_difat[fat_sector_number];
  }

  fat_sector_number -= 109;
  const std::uint32_t entriesPerSector = m_sector_size / 4 - 1;
  std::uint32_t difatSectorLocation = m_header.first_difat_sector_location;
  while (fat_sector_number >= entriesPerSector) {
    fat_sector_number -= entriesPerSector;
    const std::uint32_t address =
        sector_offset_to_address({difatSectorLocation, m_sector_size - 4});
    in.seekg(address);
    difatSectorLocation = parse_uint32(in);
  }
  const std::uint32_t address =
      sector_offset_to_address({difatSectorLocation, fat_sector_number * 4});
  in.seekg(address);
  return parse_uint32(in);
}

} // namespace odr::internal::cfb::impl
