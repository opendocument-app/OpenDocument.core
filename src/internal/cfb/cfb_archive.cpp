#include <internal/abstract/file.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/cfb/cfb_util.h>
#include <internal/util/string_util.h>
#include <odr/file.h>

namespace odr::internal::cfb {

ReadonlyCfbArchive::Entry::Entry(const ReadonlyCfbArchive &parent,
                                 const impl::CompoundFileEntry &entry)
    : m_parent{&parent}, m_entry{&entry}, m_path{"/"} {}

ReadonlyCfbArchive::Entry::Entry(const ReadonlyCfbArchive &parent,
                                 const impl::CompoundFileEntry &entry,
                                 const common::Path &parent_path)
    : m_parent{&parent}, m_entry{&entry}, m_path{parent_path.join(name())} {}

bool ReadonlyCfbArchive::Entry::operator==(const Entry &other) const {
  return m_entry == other.m_entry;
}

bool ReadonlyCfbArchive::Entry::operator!=(const Entry &other) const {
  return m_entry != other.m_entry;
}

bool ReadonlyCfbArchive::Entry::is_file() const { return m_entry->is_stream(); }

bool ReadonlyCfbArchive::Entry::is_directory() const {
  return !m_entry->is_stream();
}

common::Path ReadonlyCfbArchive::Entry::path() const { return m_path; }

std::unique_ptr<abstract::File> ReadonlyCfbArchive::Entry::file() const {
  if (!is_file()) {
    return {};
  }
  return std::make_unique<util::FileInCfb>(m_parent->m_cfb, *m_entry);
}

std::string ReadonlyCfbArchive::Entry::name() const {
  return odr::internal::util::string::c16str_to_string(
      reinterpret_cast<const char16_t *>(m_entry->name), m_entry->name_len - 2);
}

std::optional<ReadonlyCfbArchive::Entry>
ReadonlyCfbArchive::Entry::left() const {
  auto left = m_parent->m_cfb->cfb().get_entry(m_entry->left_sibling_id);
  if (left == nullptr) {
    return {};
  }
  return Entry(*m_parent, *left, m_path.parent());
}

std::optional<ReadonlyCfbArchive::Entry>
ReadonlyCfbArchive::Entry::right() const {
  auto right = m_parent->m_cfb->cfb().get_entry(m_entry->right_sibling_id);
  if (right == nullptr) {
    return {};
  }
  return Entry(*m_parent, *right, m_path.parent());
}

std::optional<ReadonlyCfbArchive::Entry>
ReadonlyCfbArchive::Entry::child() const {
  auto child = m_parent->m_cfb->cfb().get_entry(m_entry->child_id);
  if (child == nullptr) {
    return {};
  }
  return Entry(*m_parent, *child, m_path);
}

ReadonlyCfbArchive::Iterator::Iterator() = default;

ReadonlyCfbArchive::Iterator::Iterator(const ReadonlyCfbArchive &parent,
                                       const impl::CompoundFileEntry &entry)
    : m_entry{Entry(parent, entry)} {
  dig_left_();
}

ReadonlyCfbArchive::Iterator::Iterator(const ReadonlyCfbArchive &parent,
                                       const impl::CompoundFileEntry &entry,
                                       const common::Path &parent_path)
    : m_entry{Entry(parent, entry, parent_path)} {
  dig_left_();
}

void ReadonlyCfbArchive::Iterator::dig_left_() {
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

void ReadonlyCfbArchive::Iterator::next_() {
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

void ReadonlyCfbArchive::Iterator::next_flat_() {
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

ReadonlyCfbArchive::Iterator::reference
ReadonlyCfbArchive::Iterator::operator*() const {
  return *m_entry;
}

ReadonlyCfbArchive::Iterator::pointer
ReadonlyCfbArchive::Iterator::operator->() const {
  return &*m_entry;
}

bool ReadonlyCfbArchive::Iterator::operator==(const Iterator &other) const {
  return m_entry == other.m_entry;
};

bool ReadonlyCfbArchive::Iterator::operator!=(const Iterator &other) const {
  return m_entry != other.m_entry;
};

ReadonlyCfbArchive::Iterator &ReadonlyCfbArchive::Iterator::operator++() {
  next_();
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
  return Iterator(*this, *m_cfb->cfb().get_root_entry());
}

ReadonlyCfbArchive::Iterator ReadonlyCfbArchive::end() const {
  return Iterator();
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

} // namespace odr::internal::cfb
