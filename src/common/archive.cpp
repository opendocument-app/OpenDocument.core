#include <algorithm>
#include <common/archive.h>
#include <common/pointer_util.h>
#include <odr/archive.h>
#include <utility>

namespace odr::common {

class DefaultArchiveEntryIterator : public ArchiveEntryIterator {
public:
  DefaultArchiveEntryIterator(
      std::shared_ptr<const DefaultArchive> archive,
      std::vector<std::shared_ptr<DefaultArchiveEntry>>::const_iterator
          iterator)
      : m_archive{std::move(archive)}, m_iterator{std::move(iterator)} {}

  bool operator==(const ArchiveEntryIterator &rhs) const override {
    auto tmp = dynamic_cast<const DefaultArchiveEntryIterator *>(&rhs);
    if (tmp == nullptr)
      return false;
    if (m_archive != tmp->m_archive)
      return false;
    return m_iterator == tmp->m_iterator;
  }

  bool operator!=(const ArchiveEntryIterator &rhs) const override {
    return !operator==(rhs);
  }

  void next() override { ++m_iterator; }

  [[nodiscard]] std::shared_ptr<ArchiveEntry> entry() const override {
    return *m_iterator;
  }

private:
  std::shared_ptr<const DefaultArchive> m_archive;
  std::vector<std::shared_ptr<DefaultArchiveEntry>>::const_iterator m_iterator;

  friend DefaultArchive;
};

std::unique_ptr<ArchiveEntryIterator> DefaultArchive::begin() const {
  return std::make_unique<DefaultArchiveEntryIterator>(shared_from_this(),
                                                       std::begin(m_entries));
}

std::unique_ptr<ArchiveEntryIterator> DefaultArchive::end() const {
  return std::make_unique<DefaultArchiveEntryIterator>(shared_from_this(),
                                                       std::end(m_entries));
}

std::unique_ptr<ArchiveEntryIterator>
DefaultArchive::find(const common::Path &path) const {
  auto result = std::find_if(std::begin(m_entries), std::end(m_entries),
                             [&path](const auto &e) {
                               // TODO deal with relative vs absolute paths
                               return path == e->path();
                             });
  return std::make_unique<DefaultArchiveEntryIterator>(shared_from_this(),
                                                       result);
}

std::unique_ptr<ArchiveEntryIterator>
DefaultArchive::insert(std::unique_ptr<ArchiveEntryIterator> at,
                       std::unique_ptr<ArchiveEntry> entry) {
  auto at_tmp =
      dynamic_pointer_cast<DefaultArchiveEntryIterator>(std::move(at));
  if (!at_tmp)
    return {}; // TODO throw

  auto entry_tmp = dynamic_pointer_cast<DefaultArchiveEntry>(std::move(entry));
  if (!entry_tmp)
    return {}; // TODO throw

  return insert(std::move(at_tmp), std::move(entry_tmp));
}

std::unique_ptr<DefaultArchiveEntryIterator>
DefaultArchive::insert(std::unique_ptr<DefaultArchiveEntryIterator> at,
                       std::unique_ptr<DefaultArchiveEntry> entry) {
  auto result = m_entries.insert(at->m_iterator, std::move(entry));
  return std::make_unique<DefaultArchiveEntryIterator>(shared_from_this(),
                                                       result);
}

std::unique_ptr<ArchiveEntryIterator>
DefaultArchive::insert_file(std::unique_ptr<ArchiveEntryIterator> at,
                            common::Path path, std::shared_ptr<File> file) {
  return insert(std::move(at), std::make_unique<DefaultArchiveEntry>(
                                   std::move(path), std::move(file)));
}

std::unique_ptr<ArchiveEntryIterator>
DefaultArchive::insert_directory(std::unique_ptr<ArchiveEntryIterator> at,
                                 common::Path path) {
  return insert(std::move(at),
                std::make_unique<DefaultArchiveEntry>(std::move(path)));
}

void DefaultArchive::move(std::shared_ptr<ArchiveEntry> entry,
                          const common::Path &path) const {
  auto tmp = std::dynamic_pointer_cast<DefaultArchiveEntry>(entry);
  if (!tmp)
    return; // TODO throw
  // TODO check if entry is in `m_entry`
  if (*find(path) != *end())
    return; // TODO throw
  tmp->m_path = path;
}

void DefaultArchive::remove(std::shared_ptr<ArchiveEntry> entry) {
  auto it = std::find_if(std::begin(m_entries), std::end(m_entries),
                         [&entry](const auto &e) { return entry = e; });

  if (it != std::end(m_entries))
    m_entries.erase(it);
}

DefaultArchiveEntry::DefaultArchiveEntry(common::Path path)
    : DefaultArchiveEntry(std::move(path), {}) {}

DefaultArchiveEntry::DefaultArchiveEntry(common::Path path,
                                         std::shared_ptr<File> file)
    : m_path{std::move(path)}, m_file{std::move(file)} {}

ArchiveEntryType DefaultArchiveEntry::type() const {
  if (m_file)
    return ArchiveEntryType::FILE;
  return ArchiveEntryType::DIRECTORY;
}

common::Path DefaultArchiveEntry::path() const { return m_path; }

std::shared_ptr<File> DefaultArchiveEntry::open() const { return m_file; }

} // namespace odr::common
