#include <algorithm>
#include <common/archive.h>
#include <common/path.h>
#include <odr/archive.h>
#include <util/pointer_util.h>
#include <utility>

namespace odr::common {

class DefaultArchiveEntryIterator final
    : public abstract::ArchiveEntryIterator {
public:
  explicit DefaultArchiveEntryIterator(
      std::vector<std::shared_ptr<DefaultArchiveEntry>>::const_iterator
          iterator)
      : m_iterator{std::move(iterator)} {}

  [[nodiscard]] std::unique_ptr<ArchiveEntryIterator> copy() const final {
    return std::make_unique<DefaultArchiveEntryIterator>(*this);
  }

  [[nodiscard]] bool equals(const ArchiveEntryIterator &rhs) const final {
    auto tmp = dynamic_cast<const DefaultArchiveEntryIterator *>(&rhs);
    if (tmp == nullptr)
      return false;
    return m_iterator == tmp->m_iterator;
  }

  void next() final { ++m_iterator; }

  [[nodiscard]] std::shared_ptr<abstract::ArchiveEntry> entry() const final {
    return *m_iterator;
  }

private:
  std::vector<std::shared_ptr<DefaultArchiveEntry>>::const_iterator m_iterator;

  friend DefaultArchive;
};

std::unique_ptr<abstract::ArchiveEntryIterator> DefaultArchive::begin() const {
  return std::make_unique<DefaultArchiveEntryIterator>(std::begin(m_entries));
}

std::unique_ptr<abstract::ArchiveEntryIterator> DefaultArchive::end() const {
  return std::make_unique<DefaultArchiveEntryIterator>(std::end(m_entries));
}

std::unique_ptr<abstract::ArchiveEntryIterator>
DefaultArchive::find(const common::Path &path) const {
  auto result = std::find_if(std::begin(m_entries), std::end(m_entries),
                             [&path](const auto &e) {
                               // TODO deal with relative vs absolute paths
                               return path == e->path();
                             });
  return std::make_unique<DefaultArchiveEntryIterator>(result);
}

std::unique_ptr<abstract::ArchiveEntryIterator>
DefaultArchive::insert(std::unique_ptr<abstract::ArchiveEntryIterator> at,
                       std::unique_ptr<abstract::ArchiveEntry> entry) {
  auto at_tmp =
      util::dynamic_pointer_cast<DefaultArchiveEntryIterator>(std::move(at));
  if (!at_tmp) {
    return {}; // TODO throw
  }

  auto entry_tmp =
      util::dynamic_pointer_cast<DefaultArchiveEntry>(std::move(entry));
  if (!entry_tmp) {
    return {}; // TODO throw
  }

  return insert(std::move(at_tmp), std::move(entry_tmp));
}

std::unique_ptr<DefaultArchiveEntryIterator>
DefaultArchive::insert(std::unique_ptr<DefaultArchiveEntryIterator> at,
                       std::unique_ptr<DefaultArchiveEntry> entry) {
  auto result = m_entries.insert(at->m_iterator, std::move(entry));
  return std::make_unique<DefaultArchiveEntryIterator>(result);
}

std::unique_ptr<abstract::ArchiveEntryIterator>
DefaultArchive::insert_file(std::unique_ptr<abstract::ArchiveEntryIterator> at,
                            common::Path path,
                            std::shared_ptr<abstract::File> file) {
  return insert(std::move(at), std::make_unique<DefaultArchiveEntry>(
                                   std::move(path), std::move(file)));
}

std::unique_ptr<abstract::ArchiveEntryIterator>
DefaultArchive::insert_directory(
    std::unique_ptr<abstract::ArchiveEntryIterator> at, common::Path path) {
  return insert(std::move(at),
                std::make_unique<DefaultArchiveEntry>(std::move(path)));
}

void DefaultArchive::move(std::shared_ptr<abstract::ArchiveEntry> entry,
                          const common::Path &path) const {
  auto tmp = std::dynamic_pointer_cast<DefaultArchiveEntry>(entry);
  if (!tmp)
    return; // TODO throw
  // TODO check if entry is in `m_entry`
  if (!find(path)->equals(*end()))
    return; // TODO throw
  tmp->m_path = path;
}

void DefaultArchive::remove(std::shared_ptr<abstract::ArchiveEntry> entry) {
  auto it = std::find_if(std::begin(m_entries), std::end(m_entries),
                         [&entry](const auto &e) { return entry = e; });

  if (it != std::end(m_entries))
    m_entries.erase(it);
}

DefaultArchiveEntry::DefaultArchiveEntry(common::Path path)
    : DefaultArchiveEntry(std::move(path), {}) {}

DefaultArchiveEntry::DefaultArchiveEntry(common::Path path,
                                         std::shared_ptr<abstract::File> file)
    : m_path{std::move(path)}, m_file{std::move(file)} {}

ArchiveEntryType DefaultArchiveEntry::type() const {
  if (m_file)
    return ArchiveEntryType::FILE;
  return ArchiveEntryType::DIRECTORY;
}

common::Path DefaultArchiveEntry::path() const { return m_path; }

std::shared_ptr<abstract::File> DefaultArchiveEntry::open() const {
  return m_file;
}

} // namespace odr::common
