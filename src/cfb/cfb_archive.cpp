#include <abstract/file.h>
#include <cfb/cfb_archive.h>
#include <cfb/cfb_util.h>
#include <codecvt>
#include <locale>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::cfb {

namespace {
class FileInCfbIstream final : public std::istream {
public:
  FileInCfbIstream(std::shared_ptr<const ReadonlyCfbArchive> persist,
                   std::unique_ptr<util::ReaderBuffer> sbuf)
      : std::istream(sbuf.get()), m_persist{std::move(persist)},
        m_sbuf{std::move(sbuf)} {}
  FileInCfbIstream(std::shared_ptr<const ReadonlyCfbArchive> persist,
                   const impl::CompoundFileReader &reader,
                   const impl::CompoundFileEntry &entry)
      : FileInCfbIstream(std::move(persist),
                         std::make_unique<util::ReaderBuffer>(reader, entry)) {}

private:
  std::shared_ptr<const ReadonlyCfbArchive> m_persist;
  std::unique_ptr<util::ReaderBuffer> m_sbuf;
};

class FileInCfb final : public abstract::File {
public:
  FileInCfb(std::shared_ptr<const ReadonlyCfbArchive> persist,
            const FileLocation location, const impl::CompoundFileReader &reader,
            const impl::CompoundFileEntry &entry)
      : m_persist{std::move(persist)},
        m_location{location}, m_reader{reader}, m_entry{entry} {}

  [[nodiscard]] FileLocation location() const noexcept final {
    return m_location;
  }

  [[nodiscard]] std::size_t size() const final { return m_entry.size; }

  [[nodiscard]] std::unique_ptr<std::istream> read() const final {
    return std::make_unique<FileInCfbIstream>(m_persist, m_reader, m_entry);
  }

private:
  std::shared_ptr<const ReadonlyCfbArchive> m_persist;
  FileLocation m_location;
  const impl::CompoundFileReader &m_reader;
  const impl::CompoundFileEntry &m_entry;
};
} // namespace

ReadonlyCfbArchive::Entry::Entry(const impl::CompoundFileReader &reader,
                                 const impl::CompoundFileEntry *entry,
                                 common::Path path)
    : m_reader{reader}, m_entry{entry}, m_path{std::move(path)} {}

bool ReadonlyCfbArchive::Entry::is_file() const { return m_entry->is_stream(); }

bool ReadonlyCfbArchive::Entry::is_directory() const {
  return !m_entry->is_stream();
}

common::Path ReadonlyCfbArchive::Entry::path() const { return m_path; }

std::unique_ptr<abstract::File> ReadonlyCfbArchive::Entry::file() const {
  return file({});
}

std::unique_ptr<abstract::File> ReadonlyCfbArchive::Entry::file(
    std::shared_ptr<ReadonlyCfbArchive> persist) const {
  if (!is_file()) {
    return {};
  }
  return std::make_unique<FileInCfb>(std::move(persist), FileLocation::MEMORY,
                                     m_reader, *m_entry);
}

ReadonlyCfbArchive::Iterator::Iterator(const impl::CompoundFileReader &reader,
                                       const impl::CompoundFileEntry *entry,
                                       common::Path path)
    : m_entry{reader, entry, std::move(path)} {
  dig_left_();
}

void ReadonlyCfbArchive::Iterator::dig_left_() {
  while (true) {
    auto left = m_entry.m_reader.get_entry(m_entry.m_entry->left_sibling_id);
    if (left == nullptr) {
      break;
    }
    m_ancestors.push_back(m_entry.m_entry);
    m_entry.m_entry = left;
  }
}

ReadonlyCfbArchive::Iterator::reference
ReadonlyCfbArchive::Iterator::operator*() const {
  return m_entry;
}

ReadonlyCfbArchive::Iterator::pointer
ReadonlyCfbArchive::Iterator::operator->() const {
  return &m_entry;
}

bool ReadonlyCfbArchive::Iterator::operator==(const Iterator &other) const {
  return &m_entry.m_entry == &other.m_entry.m_entry;
};

bool ReadonlyCfbArchive::Iterator::operator!=(const Iterator &other) const {
  return &m_entry.m_entry != &other.m_entry.m_entry;
};

ReadonlyCfbArchive::Iterator &ReadonlyCfbArchive::Iterator::operator++() {
  auto child = m_entry.m_reader.get_entry(m_entry.m_entry->child_id);
  if (child != nullptr) {
    m_directories.push_back(m_entry.m_entry);
    m_entry.m_entry = child;
    dig_left_();
    return *this;
  }

  auto right = m_entry.m_reader.get_entry(m_entry.m_entry->right_sibling_id);
  if (right != nullptr) {
    m_entry.m_entry = right;
    dig_left_();
    return *this;
  }

  if (!m_ancestors.empty()) {
    m_entry.m_entry = m_ancestors.back();
    m_ancestors.pop_back();
    return *this;
  }

  if (!m_directories.empty()) {
    m_entry.m_entry = m_directories.back();
    m_directories.pop_back();
    return *this;
  }

  m_entry.m_entry = nullptr;
  return *this;
}

ReadonlyCfbArchive::Iterator ReadonlyCfbArchive::Iterator::operator++(int) {
  Iterator tmp = *this;
  ++(*this);
  return tmp;
}

ReadonlyCfbArchive::ReadonlyCfbArchive(std::shared_ptr<common::MemoryFile> file)
    : m_file{std::move(file)}, m_reader{m_file->content().data(),
                                        m_file->size()} {}

ReadonlyCfbArchive::Iterator ReadonlyCfbArchive::begin() const {
  return Iterator(m_reader, m_reader.get_root_entry(), "/");
}

ReadonlyCfbArchive::Iterator ReadonlyCfbArchive::end() const {
  return Iterator(m_reader, nullptr, "");
}

ReadonlyCfbArchive::Iterator
ReadonlyCfbArchive::find(const common::Path &path) const {
  for (auto it = begin(); it != end(); ++it) {
    if (it->path() == path) {
      return it;
    }
  }

  return end();
}

} // namespace odr::cfb
