#include <common/archive.h>
#include <odr/archive.h>
#include <odr/file.h>

namespace odr {

Archive::Archive() = default;

Archive::Archive(std::shared_ptr<abstract::Archive> impl)
    : m_impl{std::move(impl)} {}

ArchiveEntryIterator Archive::begin() const {
  return ArchiveEntryIterator(m_impl->begin());
}

ArchiveEntryIterator Archive::end() const {
  return ArchiveEntryIterator(m_impl->end());
}

ArchiveEntryIterator Archive::find(const std::string &path) const {
  return ArchiveEntryIterator(m_impl->find(path));
}

void Archive::move(ArchiveEntry entry, const std::string &path) const {
  m_impl->move(entry.m_impl, path);
}

void Archive::remove(ArchiveEntry entry) { m_impl->remove(entry.m_impl); }

void Archive::save(const std::string &path) const { return m_impl->save(path); }

ArchiveEntry::ArchiveEntry() = default;

ArchiveEntry::ArchiveEntry(std::shared_ptr<abstract::ArchiveEntry> impl)
    : m_impl{std::move(impl)} {}

bool ArchiveEntry::operator==(const ArchiveEntry &rhs) const {
  return m_impl == rhs.m_impl;
}

bool ArchiveEntry::operator!=(const ArchiveEntry &rhs) const {
  return m_impl != rhs.m_impl;
}

ArchiveEntry::operator bool() const { return m_impl.operator bool(); }

ArchiveEntryType ArchiveEntry::type() const { return m_impl->type(); }

std::string ArchiveEntry::path() const { return m_impl->path().string(); }

std::optional<File> ArchiveEntry::open() const {
  auto result = m_impl->open();
  if (!result)
    return {};
  return File(result);
}

ArchiveEntryIterator::ArchiveEntryIterator(
    std::shared_ptr<abstract::ArchiveEntryIterator> impl)
    : m_impl{std::move(impl)} {}

ArchiveEntryIterator &ArchiveEntryIterator::operator++() {
  m_impl->next();
  return *this;
}

ArchiveEntryIterator ArchiveEntryIterator::operator++(int) & {
  ArchiveEntryIterator result = *this;
  m_impl->next();
  return result;
}

ArchiveEntry &ArchiveEntryIterator::operator*() {
  m_entry = ArchiveEntry(m_impl->entry());
  return m_entry;
}

ArchiveEntry *ArchiveEntryIterator::operator->() {
  m_entry = ArchiveEntry(m_impl->entry());
  return &m_entry;
}

bool ArchiveEntryIterator::operator==(const ArchiveEntryIterator &rhs) const {
  return m_impl->operator==(*rhs.m_impl);
}

bool ArchiveEntryIterator::operator!=(const ArchiveEntryIterator &rhs) const {
  return m_impl->operator==(*rhs.m_impl);
}

} // namespace odr
