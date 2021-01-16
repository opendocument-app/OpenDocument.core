#include <cfb/cfb_impl.h>
#include <cfb/cfb_storage.h>
#include <cfb/cfb_util.h>
#include <codecvt>
#include <common/path.h>
#include <cstdint>
#include <locale>
#include <stdexcept>
#include <util/file_util.h>

namespace odr::cfb {

namespace {
class CfbReaderIstream final : public std::istream {
public:
  explicit CfbReaderIstream(std::unique_ptr<util::ReaderBuffer> sbuf)
      : std::istream(sbuf.get()), m_sbuf{std::move(sbuf)} {}
  CfbReaderIstream(const impl::CompoundFileReader &reader,
                   const impl::CompoundFileEntry &entry)
      : CfbReaderIstream(std::make_unique<util::ReaderBuffer>(reader, entry)) {}

private:
  std::unique_ptr<util::ReaderBuffer> m_sbuf;
};
} // namespace

typedef std::function<void(const impl::CompoundFileEntry *,
                           const common::Path &)>
    CfbVisitor;

class CfbReader::Impl final {
public:
  explicit Impl(const common::Path &path)
      : buffer{::odr::util::file::read(path)}, reader{buffer.data(),
                                                      buffer.size()} {}

  void visit(CfbVisitor visitor) const {
    reader.enum_files(
        reader.get_root_entry(), -1,
        [&](const impl::CompoundFileEntry *entry,
            const std::u16string &directory, const int level) {
          std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
              convert;
          // TODO not sure what directory is; was empty so far
          // const std::string dir = convert.to_bytes(directory);
          const std::string name = convert.to_bytes(std::u16string(
              (const char16_t *)entry->name, (entry->name_len - 1) / 2));
          visitor(entry, common::Path(name));
        });
  }

  const impl::CompoundFileEntry *find(const common::Path &p) const {
    const impl::CompoundFileEntry *result = nullptr;
    visit([&](const impl::CompoundFileEntry *entry, const common::Path &path) {
      if (p == path)
        result = entry;
    });
    return result;
  }

  bool isSomething(const common::Path &p) const { return find(p) != nullptr; }

  bool isFile(const common::Path &p) const {
    const auto entry = find(p);
    return (entry != nullptr) && reader.is_stream(entry);
  }

  bool isDirectory(const common::Path &p) const {
    const auto entry = find(p);
    return (entry != nullptr) && !reader.is_stream(entry);
  }

  bool isReadable(const common::Path &p) const { return isFile(p); }

  std::uint64_t size(const common::Path &p) const {
    const auto entry = find(p);
    if (entry == nullptr)
      return 0; // TODO throw?
    return entry->size;
  }

  void visit(Visitor visitor) const {
    visit([&](const impl::CompoundFileEntry *, const common::Path &path) {
      visitor(path);
    });
  }

  std::unique_ptr<std::istream> read(const common::Path &p) const {
    const auto entry = find(p);
    if (entry == nullptr)
      return nullptr;
    return std::make_unique<CfbReaderIstream>(reader, *entry);
  }

private:
  std::string buffer;
  impl::CompoundFileReader reader;
};

CfbReader::CfbReader(const common::Path &path)
    : impl(std::make_unique<Impl>(path)) {}

CfbReader::~CfbReader() = default;

bool CfbReader::isSomething(const common::Path &path) const {
  return impl->isSomething(path);
}

bool CfbReader::isFile(const common::Path &path) const {
  return impl->isFile(path);
}

bool CfbReader::isDirectory(const common::Path &path) const {
  return impl->isDirectory(path);
}

bool CfbReader::isReadable(const common::Path &path) const {
  return impl->isReadable(path);
}

std::uint64_t CfbReader::size(const common::Path &path) const {
  return impl->size(path);
}

void CfbReader::visit(Visitor visitor) const { impl->visit(visitor); }

std::unique_ptr<std::istream> CfbReader::read(const common::Path &path) const {
  return impl->read(path);
}

} // namespace odr::cfb
