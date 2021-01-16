#include <abstract/file.h>
#include <cfb/cfb_archive.h>
#include <cfb/cfb_file.h>
#include <cfb/cfb_util.h>
#include <codecvt>
#include <locale>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::cfb {

namespace {
class FileInCfbStreambuf final : public util::ReaderBuffer {
public:
  FileInCfbStreambuf(std::shared_ptr<const CfbFile> file,
                     const impl::CompoundFileEntry *entry)
      : ReaderBuffer(*file->impl(), *entry), m_file(std::move(file)) {}

private:
  std::shared_ptr<const CfbFile> m_file;
};

class FileInCfbIstream final : public std::istream {
public:
  explicit FileInCfbIstream(std::unique_ptr<FileInCfbStreambuf> sbuf)
      : std::istream(sbuf.get()), m_sbuf{std::move(sbuf)} {}
  FileInCfbIstream(std::shared_ptr<const CfbFile> file,
                   const impl::CompoundFileEntry *entry)
      : FileInCfbIstream(
            std::make_unique<FileInCfbStreambuf>(std::move(file), entry)) {}

private:
  std::unique_ptr<FileInCfbStreambuf> m_sbuf;
};

class FileInCfb final : public abstract::File {
public:
  FileInCfb(std::shared_ptr<const CfbFile> file,
            const impl::CompoundFileEntry *entry)
      : m_file{std::move(file)}, m_entry{entry} {}

  [[nodiscard]] FileType file_type() const noexcept final {
    return FileType::UNKNOWN;
  }

  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::UNKNOWN;
  }

  [[nodiscard]] FileMeta file_meta() const noexcept final {
    return {}; // TODO
  }

  [[nodiscard]] FileLocation file_location() const noexcept final {
    return m_file->file_location();
  }

  [[nodiscard]] std::size_t size() const final { return m_entry->size; }

  [[nodiscard]] std::unique_ptr<std::istream> data() const final {
    return std::make_unique<FileInCfbIstream>(m_file, m_entry);
  }

private:
  std::shared_ptr<const CfbFile> m_file;
  const impl::CompoundFileEntry *m_entry;
};
} // namespace

CfbArchive::CfbArchive() = default;

CfbArchive::CfbArchive(std::shared_ptr<const CfbFile> file)
    : m_file{std::move(file)} {
  auto reader = m_file->impl();
  reader->enum_files(
      reader->get_root_entry(), -1,
      [&](const impl::CompoundFileEntry *entry,
          const std::u16string & /*directory*/, const std::uint32_t /*level*/) {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
            convert;
        // TODO not sure what directory is; was empty so far
        // const std::string dir = convert.to_bytes(directory);
        const std::string path = convert.to_bytes(
            std::u16string(reinterpret_cast<const char16_t *>(entry->name),
                           (entry->name_len - 1) / 2));

        if (entry->is_stream()) {
          CfbArchive::insert_file(DefaultArchive::end(), path,
                                  nullptr); // TODO file
        } else {
          CfbArchive::insert_directory(DefaultArchive::end(), path);
        }
      });
}

void CfbArchive::save(std::ostream &) const { throw UnsupportedOperation(); }

} // namespace odr::cfb
