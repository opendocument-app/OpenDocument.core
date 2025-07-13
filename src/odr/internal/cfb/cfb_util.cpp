#include <odr/internal/cfb/cfb_util.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace odr::internal::cfb::util {

namespace {

class ReaderBuffer : public std::streambuf {
public:
  ReaderBuffer(const impl::CompoundFileReader &reader,
               const impl::CompoundFileEntry &entry)
      : m_reader{reader}, m_entry{entry}, m_buffer{new char[m_buffer_size]} {}
  ReaderBuffer(const ReaderBuffer &) = delete;
  ReaderBuffer(ReaderBuffer &&other) noexcept = delete;
  ~ReaderBuffer() override { delete[] m_buffer; }

  ReaderBuffer &operator=(const ReaderBuffer &) = delete;
  ReaderBuffer &operator=(ReaderBuffer &&other) noexcept = delete;

  int underflow() final {
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

private:
  const impl::CompoundFileReader &m_reader;
  const impl::CompoundFileEntry &m_entry;
  std::uint64_t m_offset{0};
  std::uint64_t m_buffer_size{4098};
  char *m_buffer;
};

class FileInCfbIstream final : public std::istream {
public:
  FileInCfbIstream(std::shared_ptr<const Archive> archive,
                   std::unique_ptr<util::ReaderBuffer> sbuf)
      : std::istream(sbuf.get()), m_archive{std::move(archive)},
        m_sbuf{std::move(sbuf)} {}
  FileInCfbIstream(std::shared_ptr<const Archive> archive,
                   const impl::CompoundFileReader &reader,
                   const impl::CompoundFileEntry &entry)
      : FileInCfbIstream(std::move(archive),
                         std::make_unique<util::ReaderBuffer>(reader, entry)) {}

private:
  std::shared_ptr<const Archive> m_archive;
  std::unique_ptr<util::ReaderBuffer> m_sbuf;
};

class FileInCfb final : public abstract::File {
public:
  FileInCfb(std::shared_ptr<const Archive> archive,
            const impl::CompoundFileEntry &entry)
      : m_archive{std::move(archive)}, m_entry{entry} {}

  [[nodiscard]] FileLocation location() const noexcept final {
    return m_archive->file()->location();
  }
  [[nodiscard]] std::size_t size() const final { return m_entry.size; }

  [[nodiscard]] std::optional<AbsPath> disk_path() const final {
    return std::nullopt;
  }
  [[nodiscard]] const char *memory_data() const final { return nullptr; }

  [[nodiscard]] std::unique_ptr<std::istream> stream() const final {
    return std::make_unique<FileInCfbIstream>(m_archive, m_archive->cfb(),
                                              m_entry);
  }

private:
  std::shared_ptr<const Archive> m_archive;
  const impl::CompoundFileEntry &m_entry;
};

} // namespace

bool Archive::Entry::is_file() const { return m_entry->is_stream(); }

bool Archive::Entry::is_directory() const { return !m_entry->is_stream(); }

AbsPath Archive::Entry::path() const { return m_path; }

std::unique_ptr<abstract::File> Archive::Entry::file() const {
  if (!is_file()) {
    return {};
  }
  return std::make_unique<FileInCfb>(m_parent->shared_from_this(), *m_entry);
}

std::string Archive::Entry::name() const {
  return odr::internal::util::string::c16str_to_string(
      reinterpret_cast<const char16_t *>(m_entry->name), m_entry->name_len - 2);
}

std::optional<Archive::Entry> Archive::Entry::left() const {
  auto left = m_parent->cfb().get_entry(m_entry->left_sibling_id);
  if (left == nullptr) {
    return {};
  }
  return Entry(*m_parent, *left, m_path.parent());
}

std::optional<Archive::Entry> Archive::Entry::right() const {
  auto right = m_parent->cfb().get_entry(m_entry->right_sibling_id);
  if (right == nullptr) {
    return {};
  }
  return Entry(*m_parent, *right, m_path.parent());
}

std::optional<Archive::Entry> Archive::Entry::child() const {
  auto child = m_parent->cfb().get_entry(m_entry->child_id);
  if (child == nullptr) {
    return {};
  }
  return Entry(*m_parent, *child, m_path);
}

void Archive::Iterator::dig_left_() {
  if (!m_entry) {
    return;
  }

  while (true) {
    auto left = m_entry->left();
    if (!left) {
      break;
    }
    m_ancestors.push_back(*m_entry);
    m_entry = left;
  }
}

void Archive::Iterator::next_() {
  if (!m_entry) {
    return;
  }

  auto child = m_entry->child();
  if (child) {
    m_directories.push_back(*m_entry);
    m_entry = child;
    dig_left_();
    return;
  }

  next_flat_();
}

void Archive::Iterator::next_flat_() {
  if (!m_entry) {
    return;
  }

  auto right = m_entry->right();
  if (right) {
    m_entry = right;
    dig_left_();
    return;
  }

  if (!m_ancestors.empty()) {
    m_entry = m_ancestors.back();
    m_ancestors.pop_back();
    return;
  }

  if (!m_directories.empty()) {
    m_entry = m_directories.back();
    m_directories.pop_back();
    next_flat_();
    return;
  }

  m_entry = {};
}

Archive::Archive(const std::shared_ptr<MemoryFile> &file)
    : m_file{file}, m_cfb{file->content().data(), file->content().size()} {}

const impl::CompoundFileReader &Archive::cfb() const { return m_cfb; }

std::shared_ptr<abstract::File> Archive::file() const { return m_file; }

Archive::Iterator Archive::begin() const {
  return {*this, *m_cfb.get_root_entry()};
}

Archive::Iterator Archive::end() const { return {}; }

Archive::Iterator Archive::find(const AbsPath &path) const {
  return std::find_if(begin(), end(), [&path](const Entry &entry) {
    return entry.path() == path;
  });
}

} // namespace odr::internal::cfb::util
