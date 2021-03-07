#include <algorithm>
#include <internal/cfb/cfb_impl.h>
#include <internal/cfb/cfb_util.h>
#include <internal/common/file.h>
#include <streambuf>

namespace odr::internal::cfb::util {

namespace {

class ReaderBuffer : public std::streambuf {
public:
  ReaderBuffer(const impl::CompoundFileReader &reader,
               const impl::CompoundFileEntry &entry);
  ~ReaderBuffer() override;

  int underflow() final;

private:
  const impl::CompoundFileReader &m_reader;
  const impl::CompoundFileEntry &m_entry;
  std::uint64_t m_offset{0};
  std::uint64_t m_buffer_size{4098};
  char *m_buffer;
};

class FileInCfbIstream final : public std::istream {
public:
  FileInCfbIstream(std::shared_ptr<Archive> archive,
                   std::unique_ptr<util::ReaderBuffer> sbuf);
  FileInCfbIstream(std::shared_ptr<Archive> archive,
                   const impl::CompoundFileReader &reader,
                   const impl::CompoundFileEntry &entry);

private:
  std::shared_ptr<Archive> m_archive;
  std::unique_ptr<util::ReaderBuffer> m_sbuf;
};

FileInCfbIstream::FileInCfbIstream(std::shared_ptr<Archive> archive,
                                   std::unique_ptr<util::ReaderBuffer> sbuf)
    : std::istream(sbuf.get()), m_archive{std::move(archive)}, m_sbuf{std::move(
                                                                   sbuf)} {}

FileInCfbIstream::FileInCfbIstream(std::shared_ptr<Archive> archive,
                                   const impl::CompoundFileReader &reader,
                                   const impl::CompoundFileEntry &entry)
    : FileInCfbIstream(std::move(archive),
                       std::make_unique<util::ReaderBuffer>(reader, entry)) {}

ReaderBuffer::ReaderBuffer(const impl::CompoundFileReader &reader,
                           const impl::CompoundFileEntry &entry)
    : m_reader{reader}, m_entry{entry}, m_buffer{new char[m_buffer_size]} {}

ReaderBuffer::~ReaderBuffer() { delete[] m_buffer; }

int ReaderBuffer::underflow() {
  const std::uint64_t remaining = m_entry.size - m_offset;
  if (remaining <= 0) {
    return std::char_traits<char>::eof();
  }

  const std::uint64_t amount = std::min(remaining, m_buffer_size);
  m_reader.read_file(&m_entry, m_offset, m_buffer, amount);
  m_offset += amount;
  setg(m_buffer, m_buffer, m_buffer + amount);

  return std::char_traits<char>::to_int_type(*gptr());
}

} // namespace

Archive::Archive(const std::shared_ptr<common::MemoryFile> &file)
    : m_cfb{file->content().data(), file->content().size()}, m_file{file} {}

const impl::CompoundFileReader &Archive::cfb() const { return m_cfb; }

std::shared_ptr<abstract::File> Archive::file() const { return m_file; }

FileInCfb::FileInCfb(std::shared_ptr<Archive> archive,
                     const impl::CompoundFileEntry &entry)
    : m_archive{std::move(archive)}, m_entry{entry} {}

[[nodiscard]] experimental::FileLocation FileInCfb::location() const noexcept {
  return m_archive->file()->location();
}

[[nodiscard]] std::size_t FileInCfb::size() const { return m_entry.size; }

[[nodiscard]] std::unique_ptr<std::istream> FileInCfb::read() const {
  return std::make_unique<FileInCfbIstream>(m_archive, m_archive->cfb(),
                                            m_entry);
}

} // namespace odr::internal::cfb::util
