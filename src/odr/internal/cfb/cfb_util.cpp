#include <odr/internal/cfb/cfb_util.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace odr::internal::cfb::util {

namespace {

class ReaderBuffer final : public std::streambuf {
public:
  ReaderBuffer(const impl::CompoundFileReader &reader,
               const impl::CompoundFileEntry &entry,
               std::unique_ptr<std::istream> stream,
               const std::uint32_t buffer_size = 4098)
      : m_reader{&reader}, m_entry{&entry}, m_stream{std::move(stream)},
        m_buffer(buffer_size, '\0') {}

  int underflow() override {
    if (m_offset >= m_entry->size) {
      return traits_type::eof();
    }

    const std::uint64_t remaining = m_entry->size - m_offset;
    const std::uint64_t amount =
        std::min<std::uint64_t>(remaining, m_buffer.size());
    m_reader->read_file(*m_stream, *m_entry, m_offset, m_buffer.data(), amount);
    m_offset += amount;
    setg(m_buffer.data(), m_buffer.data(), m_buffer.data() + amount);

    return traits_type::to_int_type(*gptr());
  }

private:
  const impl::CompoundFileReader *m_reader{};
  const impl::CompoundFileEntry *m_entry{};
  std::unique_ptr<std::istream> m_stream;
  std::uint64_t m_offset{0};
  std::vector<char> m_buffer;
};

class FileInCfbIstream final : public std::istream {
public:
  FileInCfbIstream(std::shared_ptr<const Archive> archive,
                   std::unique_ptr<ReaderBuffer> sbuf)
      : std::istream(sbuf.get()), m_archive{std::move(archive)},
        m_sbuf{std::move(sbuf)} {}
  FileInCfbIstream(const std::shared_ptr<const Archive> &archive,
                   const impl::CompoundFileReader &reader,
                   const impl::CompoundFileEntry &entry)
      : FileInCfbIstream(archive,
                         std::make_unique<ReaderBuffer>(
                             reader, entry, archive->file()->stream())) {}

private:
  std::shared_ptr<const Archive> m_archive;
  std::unique_ptr<ReaderBuffer> m_sbuf;
};

class FileInCfb final : public abstract::File {
public:
  FileInCfb(std::shared_ptr<const Archive> archive,
            const impl::CompoundFileEntry &entry)
      : m_archive{std::move(archive)}, m_entry{entry} {}

  [[nodiscard]] FileLocation location() const noexcept override {
    return m_archive->file()->location();
  }
  [[nodiscard]] std::size_t size() const override { return m_entry.size; }

  [[nodiscard]] std::optional<AbsPath> disk_path() const override {
    return std::nullopt;
  }
  [[nodiscard]] const char *memory_data() const override { return nullptr; }

  [[nodiscard]] std::unique_ptr<std::istream> stream() const override {
    return std::make_unique<FileInCfbIstream>(m_archive, m_archive->cfb(),
                                              m_entry);
  }

private:
  std::shared_ptr<const Archive> m_archive;
  impl::CompoundFileEntry m_entry;
};

} // namespace

Archive::Archive(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)}, m_cfb{*m_file->stream(), m_file->size()} {}

bool Archive::Entry::is_file() const { return m_entry.is_stream(); }

bool Archive::Entry::is_directory() const { return !m_entry.is_stream(); }

AbsPath Archive::Entry::path() const { return m_path; }

std::unique_ptr<abstract::File> Archive::Entry::file() const {
  if (!is_file()) {
    return {};
  }
  return std::make_unique<FileInCfb>(m_archive->shared_from_this(), m_entry);
}

std::string Archive::Entry::name() const {
  return internal::util::string::c16str_to_string(m_entry.name,
                                                  m_entry.name_len - 2);
}

std::optional<Archive::Entry> Archive::Entry::left() const {
  if (m_entry.left_sibling_id == impl::NullId) {
    return {};
  }
  const impl::CompoundFileEntry left = m_archive->m_cfb.parse_entry(
      *m_archive->m_file->stream(), m_entry.left_sibling_id);
  return Entry(*m_archive, m_entry.left_sibling_id, left, m_path.parent());
}

std::optional<Archive::Entry> Archive::Entry::right() const {
  if (m_entry.right_sibling_id == impl::NullId) {
    return {};
  }
  const impl::CompoundFileEntry right = m_archive->m_cfb.parse_entry(
      *m_archive->m_file->stream(), m_entry.right_sibling_id);
  return Entry(*m_archive, m_entry.right_sibling_id, right, m_path.parent());
}

std::optional<Archive::Entry> Archive::Entry::child() const {
  if (m_entry.child_id == impl::NullId) {
    return {};
  }
  const impl::CompoundFileEntry child = m_archive->m_cfb.parse_entry(
      *m_archive->m_file->stream(), m_entry.child_id);
  return Entry(*m_archive, m_entry.child_id, child, m_path);
}

void Archive::Iterator::dig_left_() {
  if (!m_entry.has_value()) {
    return;
  }

  while (true) {
    const std::optional<Entry> left = m_entry->left();
    if (!left.has_value()) {
      break;
    }
    m_ancestors.push_back(*m_entry);
    m_entry = left;
  }
}

void Archive::Iterator::next_() {
  if (!m_entry.has_value()) {
    return;
  }

  if (const std::optional<Entry> child = m_entry->child(); child.has_value()) {
    m_directories.push_back(*m_entry);
    m_entry = child;
    dig_left_();
    return;
  }

  next_flat_();
}

void Archive::Iterator::next_flat_() {
  if (!m_entry.has_value()) {
    return;
  }

  if (const std::optional<Entry> right = m_entry->right(); right.has_value()) {
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

const impl::CompoundFileReader &Archive::cfb() const { return m_cfb; }

std::shared_ptr<abstract::File> Archive::file() const { return m_file; }

Archive::Iterator Archive::begin() const {
  return {*this, impl::RootId, m_cfb.get_root_entry()};
}

Archive::Iterator Archive::end() const { return {}; }

Archive::Iterator Archive::find(const AbsPath &path) const {
  return std::find_if(begin(), end(), [&path](const Entry &entry) {
    return entry.path() == path;
  });
}

} // namespace odr::internal::cfb::util
