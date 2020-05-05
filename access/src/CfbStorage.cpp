#include <access/CfbStorage.h>
#include <access/FileUtil.h>
#include <access/Path.h>
#include <algorithm>
#include <codecvt>
#include <cstdint>
#include <cstring>
#include <functional>
#include <locale>
#include <stdexcept>

namespace odr {
namespace access {

namespace {
namespace CFB {

constexpr auto MAGIC = "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1";
constexpr std::size_t MAX_REG_SECT = 0xFFFFFFFA;

#pragma pack(push, 1)

struct CompoundFileHeader {
  std::uint8_t signature[8];
  std::uint8_t unused_clsid[16];
  std::uint16_t minorVersion;
  std::uint16_t majorVersion;
  std::uint16_t byteOrder;
  std::uint16_t sectorShift;
  std::uint16_t miniSectorShift;
  std::uint8_t reserved[6];
  std::uint32_t numDirectorySector;
  std::uint32_t numFATSector;
  std::uint32_t firstDirectorySectorLocation;
  std::uint32_t transactionSignatureNumber;
  std::uint32_t miniStreamCutoffSize;
  std::uint32_t firstMiniFATSectorLocation;
  std::uint32_t numMiniFATSector;
  std::uint32_t firstDIFATSectorLocation;
  std::uint32_t numDIFATSector;
  std::uint32_t headerDIFAT[109];
};

struct CompoundFileEntry {
  std::uint16_t name[32];
  std::uint16_t nameLen;
  std::uint8_t type;
  std::uint8_t colorFlag;
  std::uint32_t leftSiblingId;
  std::uint32_t rightSiblingId;
  std::uint32_t childId;
  std::uint8_t clsid[16];
  std::uint32_t stateBits;
  std::uint64_t creationTime;
  std::uint64_t modifiedTime;
  std::uint32_t startSectorLocation;
  std::uint64_t size;
};

struct PropertySetStreamHeader {
  std::uint8_t byteOrder[2];
  std::uint16_t version;
  std::uint32_t systemIdentifier;
  std::uint8_t clsid[16];
  std::uint32_t numPropertySets;
  struct {
    char fmtid[16];
    std::uint32_t offset;
  } propertySetInfo[1];
};

struct PropertySetHeader {
  std::uint32_t size;
  std::uint32_t numProperties;
  struct {
    std::uint32_t id;
    std::uint32_t offset;
  } propertyIdentifierAndOffset[1];
};

#pragma pack(pop)

std::uint32_t ParseUint32(const void *buffer) {
  return *static_cast<const std::uint32_t *>(buffer);
}

typedef std::function<void(const CompoundFileEntry *, const std::u16string &dir,
                           int level)>
    EnumFilesCallback;

class CompoundFileReader {
public:
  CompoundFileReader(const void *buffer, const std::size_t len)
      : m_buffer(static_cast<const std::uint8_t *>(buffer)), m_bufferLen(len),
        m_hdr(static_cast<const CompoundFileHeader *>(buffer)),
        m_sectorSize(512), m_miniSectorSize(64), m_miniStreamStartSector(0) {
    if (buffer == nullptr || len == 0)
      throw std::invalid_argument("");

    if (m_bufferLen < sizeof(*m_hdr) ||
        std::memcmp(m_hdr->signature, MAGIC, 8) != 0) {
      throw NoCfbFileException();
    }

    m_sectorSize = m_hdr->majorVersion == 3 ? 512 : 4096;

    // The file must contains at least 3 sectors
    if (m_bufferLen < m_sectorSize * 3)
      throw CfbFileCorruptedException();

    const CompoundFileEntry *root = GetEntry(0);
    if (root == nullptr)
      throw CfbFileCorruptedException();

    m_miniStreamStartSector = root->startSectorLocation;
  }

  /// Get entry (directory or file) by its ID.
  /// Pass "0" to get the root directory entry. -- This is the start point to
  /// navigate the compound file. Use the returned object to access child
  /// entries.
  const CompoundFileEntry *GetEntry(const std::size_t entryID) const {
    if (entryID == 0xFFFFFFFF) {
      return nullptr;
    }

    if (m_bufferLen / sizeof(CompoundFileEntry) <= entryID) {
      throw std::invalid_argument("");
    }

    std::size_t sector = 0;
    std::size_t offset = 0;
    LocateFinalSector(m_hdr->firstDirectorySectorLocation,
                      entryID * sizeof(CompoundFileEntry), &sector, &offset);
    return reinterpret_cast<const CompoundFileEntry *>(
        SectorOffsetToAddress(sector, offset));
  }

  const CompoundFileEntry *GetRootEntry() const { return GetEntry(0); }

  const CompoundFileHeader *GetFileInfo() const { return m_hdr; }

  /// Get file(stream) data start with "offset".
  /// The buffer must have enough space to store "len" bytes. Typically "len" is
  /// derived by the steam length.
  void ReadFile(const CompoundFileEntry *entry, const std::size_t offset,
                char *buffer, const std::size_t len) const {
    if (entry->size < offset || entry->size - offset < len)
      throw std::invalid_argument("");

    if (entry->size < m_hdr->miniStreamCutoffSize) {
      ReadMiniStream(entry->startSectorLocation, offset, buffer, len);
    } else {
      ReadStream(entry->startSectorLocation, offset, buffer, len);
    }
  }

  bool IsPropertyStream(const CompoundFileEntry *entry) const {
    // defined in [MS-OLEPS] 2.23 "Property Set Stream and Storage Names"
    return entry->name[0] == 5;
  }

  bool IsStream(const CompoundFileEntry *entry) const {
    return entry->type == 2;
  }

  void EnumFiles(const CompoundFileEntry *entry, int maxLevel,
                 EnumFilesCallback callback) const {
    std::u16string dir;
    EnumNodes(GetEntry(entry->childId), 0, maxLevel, dir, callback);
  }

private:
  // Enum entries with same level, including 'entry' itself
  void EnumNodes(const CompoundFileEntry *entry, const int currentLevel,
                 const int maxLevel, const std::u16string &dir,
                 EnumFilesCallback callback) const {
    if (maxLevel > 0 && currentLevel >= maxLevel)
      return;
    if (entry == nullptr)
      return;

    callback(entry, dir, currentLevel + 1);

    const CompoundFileEntry *child = GetEntry(entry->childId);
    if (child != nullptr) {
      std::u16string newDir = dir;
      if (dir.length() != 0)
        newDir.append(1, '\n');
      newDir.append((char16_t *)entry->name, entry->nameLen / 2);
      EnumNodes(GetEntry(entry->childId), currentLevel + 1, maxLevel, newDir,
                callback);
    }

    EnumNodes(GetEntry(entry->leftSiblingId), currentLevel, maxLevel, dir,
              callback);
    EnumNodes(GetEntry(entry->rightSiblingId), currentLevel, maxLevel, dir,
              callback);
  }

  void ReadStream(std::size_t sector, std::size_t offset, char *buffer,
                  std::size_t len) const {
    LocateFinalSector(sector, offset, &sector, &offset);

    // copy as many as possible in each step
    // copylen typically iterate as: m_sectorSize - offset   -->   m_sectorSize
    // -->   m_sectorSize  --> ... -->    remaining
    while (len > 0) {
      const std::uint8_t *src = SectorOffsetToAddress(sector, offset);
      std::size_t copylen = std::min(len, m_sectorSize - offset);
      if (m_buffer + m_bufferLen < src + copylen)
        throw CfbFileCorruptedException();

      std::memcpy(buffer, src, copylen);
      buffer += copylen;
      len -= copylen;
      sector = GetNextSector(sector);
      offset = 0;
    }
  }

  // Same logic as "ReadStream" except that use MiniStream functions instead
  void ReadMiniStream(std::size_t sector, std::size_t offset, char *buffer,
                      std::size_t len) const {
    LocateFinalMiniSector(sector, offset, &sector, &offset);

    // copy as many as possible in each step
    // copylen typically iterate as: m_sectorSize - offset   -->   m_sectorSize
    // -->   m_sectorSize  --> ... -->    remaining
    while (len > 0) {
      const std::uint8_t *src = MiniSectorOffsetToAddress(sector, offset);
      std::size_t copylen = std::min(len, m_miniSectorSize - offset);
      if (m_buffer + m_bufferLen < src + copylen)
        throw CfbFileCorruptedException();

      std::memcpy(buffer, src, copylen);
      buffer += copylen;
      len -= copylen;
      sector = GetNextMiniSector(sector);
      offset = 0;
    }
  }

  std::size_t GetNextSector(std::size_t sector) const {
    // lookup FAT
    std::size_t entriesPerSector = m_sectorSize / 4;
    std::size_t fatSectorNumber = sector / entriesPerSector;
    std::size_t fatSectorLocation = GetFATSectorLocation(fatSectorNumber);
    return ParseUint32(SectorOffsetToAddress(fatSectorLocation,
                                             sector % entriesPerSector * 4));
  }

  std::size_t GetNextMiniSector(std::size_t miniSector) const {
    std::size_t sector, offset;
    LocateFinalSector(m_hdr->firstMiniFATSectorLocation, miniSector * 4,
                      &sector, &offset);
    return ParseUint32(SectorOffsetToAddress(sector, offset));
  }

  // Get absolute address from sector and offset.
  const std::uint8_t *SectorOffsetToAddress(std::size_t sector,
                                            std::size_t offset) const {
    if (sector >= MAX_REG_SECT || offset >= m_sectorSize ||
        m_bufferLen <= static_cast<std::uint64_t>(m_sectorSize) * sector +
                           m_sectorSize + offset) {
      throw CfbFileCorruptedException();
    }

    return m_buffer + m_sectorSize + m_sectorSize * sector + offset;
  }

  const std::uint8_t *MiniSectorOffsetToAddress(std::size_t sector,
                                                std::size_t offset) const {
    if (sector >= MAX_REG_SECT || offset >= m_miniSectorSize ||
        m_bufferLen <=
            static_cast<std::uint64_t>(m_miniSectorSize) * sector + offset) {
      throw CfbFileCorruptedException();
    }

    LocateFinalSector(m_miniStreamStartSector,
                      sector * m_miniSectorSize + offset, &sector, &offset);
    return SectorOffsetToAddress(sector, offset);
  }

  // Locate the final sector/offset when original offset expands multiple
  // sectors
  void LocateFinalSector(std::size_t sector, std::size_t offset,
                         std::size_t *finalSector,
                         std::size_t *finalOffset) const {
    while (offset >= m_sectorSize) {
      offset -= m_sectorSize;
      sector = GetNextSector(sector);
    }
    *finalSector = sector;
    *finalOffset = offset;
  }

  void LocateFinalMiniSector(std::size_t sector, std::size_t offset,
                             std::size_t *finalSector,
                             std::size_t *finalOffset) const {
    while (offset >= m_miniSectorSize) {
      offset -= m_miniSectorSize;
      sector = GetNextMiniSector(sector);
    }
    *finalSector = sector;
    *finalOffset = offset;
  }

  std::size_t GetFATSectorLocation(std::size_t fatSectorNumber) const {
    if (fatSectorNumber < 109) {
      return m_hdr->headerDIFAT[fatSectorNumber];
    } else {
      fatSectorNumber -= 109;
      std::size_t entriesPerSector = m_sectorSize / 4 - 1;
      std::size_t difatSectorLocation = m_hdr->firstDIFATSectorLocation;
      while (fatSectorNumber >= entriesPerSector) {
        fatSectorNumber -= entriesPerSector;
        const std::uint8_t *addr =
            SectorOffsetToAddress(difatSectorLocation, m_sectorSize - 4);
        difatSectorLocation = ParseUint32(addr);
      }
      return ParseUint32(
          SectorOffsetToAddress(difatSectorLocation, fatSectorNumber * 4));
    }
  }

private:
  const std::uint8_t *m_buffer;
  std::size_t m_bufferLen;

  const CompoundFileHeader *m_hdr;
  std::size_t m_sectorSize;
  std::size_t m_miniSectorSize;
  std::size_t m_miniStreamStartSector;
};

class PropertySet final {
public:
  PropertySet(const void *buffer, std::size_t len, const char *fmtid)
      : m_buffer(static_cast<const std::uint8_t *>(buffer)), m_bufferLen(len),
        m_hdr(reinterpret_cast<const PropertySetHeader *>(buffer)),
        m_fmtid(fmtid) {
    if (m_bufferLen < sizeof(*m_hdr) ||
        m_bufferLen < sizeof(*m_hdr) +
                          (m_hdr->numProperties - 1) *
                              sizeof(m_hdr->propertyIdentifierAndOffset[0])) {
      throw CfbFileCorruptedException();
    }
  }

  /// return the string property in UTF-16 format
  const std::uint16_t *GetStringProperty(std::uint32_t propertyID) {
    for (std::uint32_t i = 0; i < m_hdr->numProperties; i++) {
      if (m_hdr->propertyIdentifierAndOffset[i].id == propertyID) {
        std::uint32_t offset = m_hdr->propertyIdentifierAndOffset[i].offset;
        if (m_bufferLen < offset + 8)
          throw CfbFileCorruptedException();
        std::uint32_t stringLengthInChar = ParseUint32(m_buffer + offset + 4);
        if (m_bufferLen < offset + 8 + stringLengthInChar * 2)
          throw CfbFileCorruptedException();
        return reinterpret_cast<const std::uint16_t *>(m_buffer + offset + 8);
      }
    }

    return nullptr;
  }

  /// Note: Getting property of types other than "string" is not implemented
  /// yet.
  ///       However most other types are simpler than string so can be easily
  ///       added. see [MS-OLEPS]

  const char *GetFmtID() { return m_fmtid; }

private:
  const std::uint8_t *m_buffer;
  std::size_t m_bufferLen;
  const PropertySetHeader *m_hdr;
  const char *m_fmtid; // 16 bytes
};

class PropertySetStream final {
public:
  PropertySetStream(const void *buffer, std::size_t len)
      : m_buffer(static_cast<const std::uint8_t *>(buffer)), m_bufferLen(len),
        m_hdr(reinterpret_cast<const PropertySetStreamHeader *>(buffer)) {
    if (m_bufferLen < sizeof(*m_hdr) ||
        m_bufferLen < sizeof(*m_hdr) + (m_hdr->numPropertySets - 1) *
                                           sizeof(m_hdr->propertySetInfo[0])) {
      throw CfbFileCorruptedException();
    }
  }

  std::size_t GetPropertySetCount() { return m_hdr->numPropertySets; }

  PropertySet GetPropertySet(std::size_t index) {
    if (index >= GetPropertySetCount())
      throw CfbFileCorruptedException();
    std::uint32_t offset = m_hdr->propertySetInfo[index].offset;
    if (m_bufferLen < offset + 4)
      throw CfbFileCorruptedException();
    std::uint32_t size = ParseUint32(m_buffer + offset);
    if (m_bufferLen < offset + size)
      throw CfbFileCorruptedException();
    return {m_buffer + offset, size, m_hdr->propertySetInfo[index].fmtid};
  }

private:
  const std::uint8_t *m_buffer;
  std::size_t m_bufferLen;
  const PropertySetStreamHeader *m_hdr;
};

} // namespace CFB
} // namespace

namespace {
constexpr std::uint64_t buffer_size_ = 4098;

class CfbReaderBuf final : public std::streambuf {
public:
  CfbReaderBuf(const CFB::CompoundFileReader &reader,
               const CFB::CompoundFileEntry &entry)
      : reader_(reader), entry_(entry), buffer_(new char[buffer_size_]) {}

  ~CfbReaderBuf() final { delete[] buffer_; }

  int underflow() final {
    const std::uint64_t remaining = entry_.size - offset_;
    if (remaining <= 0)
      return std::char_traits<char>::eof();

    const std::uint64_t amount = std::min(remaining, buffer_size_);
    reader_.ReadFile(&entry_, offset_, buffer_, amount);
    offset_ += amount;
    this->setg(this->buffer_, this->buffer_, this->buffer_ + amount);

    return std::char_traits<char>::to_int_type(*gptr());
  }

private:
  const CFB::CompoundFileReader &reader_;
  const CFB::CompoundFileEntry &entry_;
  std::uint64_t offset_{0};
  char *buffer_;
};

class CfbReaderIstream final : public std::istream {
public:
  CfbReaderIstream(const CFB::CompoundFileReader &reader,
                   const CFB::CompoundFileEntry &entry)
      : CfbReaderIstream(new CfbReaderBuf(reader, entry)) {}
  explicit CfbReaderIstream(CfbReaderBuf *sbuf)
      : std::istream(sbuf), sbuf_(sbuf) {}
  ~CfbReaderIstream() final { delete sbuf_; }

private:
  CfbReaderBuf *sbuf_;
};
} // namespace

typedef std::function<void(const CFB::CompoundFileEntry *, const Path &)>
    CfbVisitor;

class CfbReader::Impl final {
public:
  explicit Impl(const Path &path)
      : buffer(FileUtil::read(path)), reader(buffer.data(), buffer.size()) {}

  void visit(CfbVisitor visitor) const {
    reader.EnumFiles(
        reader.GetRootEntry(), -1,
        [&](const CFB::CompoundFileEntry *entry,
            const std::u16string &directory, const int level) {
          std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
              convert;
          // TODO not sure what directory is; was empty so far
          // const std::string dir = convert.to_bytes(directory);
          const std::string name = convert.to_bytes(std::u16string(
              (const char16_t *)entry->name, (entry->nameLen - 1) / 2));
          visitor(entry, Path(name));
        });
  }

  const CFB::CompoundFileEntry *find(const Path &p) const {
    const CFB::CompoundFileEntry *result = nullptr;
    visit([&](const CFB::CompoundFileEntry *entry, const Path &path) {
      if (p == path)
        result = entry;
    });
    return result;
  }

  bool isSomething(const Path &p) const { return find(p) != nullptr; }

  bool isFile(const Path &p) const {
    const auto entry = find(p);
    return (entry != nullptr) && reader.IsStream(entry);
  }

  bool isDirectory(const Path &p) const {
    const auto entry = find(p);
    return (entry != nullptr) && !reader.IsStream(entry);
  }

  bool isReadable(const Path &p) const { return isFile(p); }

  std::uint64_t size(const Path &p) const {
    const auto entry = find(p);
    if (entry == nullptr)
      return 0; // TODO throw?
    return entry->size;
  }

  void visit(Visitor visitor) const {
    visit([&](const CFB::CompoundFileEntry *, const Path &path) {
      visitor(path);
    });
  }

  std::unique_ptr<std::istream> read(const Path &p) const {
    const auto entry = find(p);
    if (entry == nullptr)
      return nullptr;
    return std::make_unique<CfbReaderIstream>(reader, *entry);
  }

private:
  std::string buffer;
  CFB::CompoundFileReader reader;
};

CfbReader::CfbReader(const Path &path) : impl(std::make_unique<Impl>(path)) {}

CfbReader::~CfbReader() = default;

bool CfbReader::isSomething(const Path &path) const {
  return impl->isSomething(path);
}

bool CfbReader::isFile(const Path &path) const { return impl->isFile(path); }

bool CfbReader::isDirectory(const Path &path) const {
  return impl->isDirectory(path);
}

bool CfbReader::isReadable(const Path &path) const {
  return impl->isReadable(path);
}

std::uint64_t CfbReader::size(const Path &path) const {
  return impl->size(path);
}

void CfbReader::visit(Visitor visitor) const { impl->visit(visitor); }

std::unique_ptr<std::istream> CfbReader::read(const Path &path) const {
  return impl->read(path);
}

} // namespace access
} // namespace odr
