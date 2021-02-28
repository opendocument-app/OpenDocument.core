#include <abstract/file.h>
#include <cfb/cfb_archive.h>
#include <cfb/cfb_util.h>
#include <codecvt>
#include <locale>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::cfb {

ReadonlyCfbArchive::Entry::Entry(const ReadonlyCfbArchive &parent,
                                 const impl::CompoundFileEntry *entry,
                                 common::Path path)
    : m_parent{parent}, m_entry{entry}, m_path{std::move(path)} {}

bool ReadonlyCfbArchive::Entry::is_file() const { return m_entry->is_stream(); }

bool ReadonlyCfbArchive::Entry::is_directory() const {
  return !m_entry->is_stream();
}

common::Path ReadonlyCfbArchive::Entry::path() const { return m_path; }

std::unique_ptr<abstract::File> ReadonlyCfbArchive::Entry::file() const {
  if (!is_file()) {
    return {};
  }
  return std::make_unique<util::FileInCfb>(m_parent.m_cfb, *m_entry);
}

ReadonlyCfbArchive::Iterator::Iterator(const ReadonlyCfbArchive &parent,
                                       const impl::CompoundFileEntry *entry,
                                       common::Path path)
    : m_entry{parent, entry, std::move(path)} {
  dig_left_();
}

void ReadonlyCfbArchive::Iterator::dig_left_() {
  while (true) {
    auto left = m_entry.m_parent.m_cfb->cfb().get_entry(
        m_entry.m_entry->left_sibling_id);
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
  auto child =
      m_entry.m_parent.m_cfb->cfb().get_entry(m_entry.m_entry->child_id);
  if (child != nullptr) {
    m_directories.push_back(m_entry.m_entry);
    m_entry.m_entry = child;
    dig_left_();
    return *this;
  }

  auto right = m_entry.m_parent.m_cfb->cfb().get_entry(
      m_entry.m_entry->right_sibling_id);
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

ReadonlyCfbArchive::ReadonlyCfbArchive(
    const std::shared_ptr<common::MemoryFile> &file)
    : m_cfb{std::make_shared<util::Archive>(file)} {}

ReadonlyCfbArchive::Iterator ReadonlyCfbArchive::begin() const {
  return Iterator(*this, m_cfb->cfb().get_root_entry(), "/");
}

ReadonlyCfbArchive::Iterator ReadonlyCfbArchive::end() const {
  return Iterator(*this, nullptr, "");
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
