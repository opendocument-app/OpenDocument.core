#ifndef ODR_COMMON_ARCHIVE_H
#define ODR_COMMON_ARCHIVE_H

#include <abstract/archive.h>
#include <common/path.h>
#include <memory>
#include <vector>

namespace odr::common {

struct ArchiveInternalEntryTemplate {
  common::Path path;
  ArchiveEntryType type;
  std::shared_ptr<abstract::File> file;
};

template <typename InternalEntry>
class ArchiveEntryTemplate : public abstract::ArchiveEntry {
public:
  ArchiveEntryTemplate(std::shared_ptr<abstract::ArchiveFile> archive,
                       InternalEntry &entry)
      : m_archive{std::move(archive)}, m_entry{entry} {}

  [[nodiscard]] bool equals(const ArchiveEntry &rhs) const override {
    auto *other = dynamic_cast<ArchiveEntryTemplate<InternalEntry>>(&rhs);
    if (other == nullptr) {
      return false;
    }
    if (m_archive != other->m_archive) {
      return false;
    }
    if (&m_entry != &other->m_entry) {
      return false;
    }
    return true;
  }

  [[nodiscard]] std::shared_ptr<abstract::ArchiveEntry> next() const override {
    if (m_entry.next == nullptr) {
      return {};
    }
    return std::make_shared<ArchiveEntryTemplate<InternalEntry>>(m_archive,
                                                                 *m_entry.next);
  }

  [[nodiscard]] ArchiveEntryType type() const override { return m_entry.type; }

  [[nodiscard]] common::Path path() const override { return m_entry.path; }

  [[nodiscard]] std::shared_ptr<abstract::File> file() const override {
    return m_entry.file;
  }

  void file(std::shared_ptr<abstract::File> file) override {
    m_entry.file = file;
  }

private:
  std::shared_ptr<abstract::ArchiveFile> m_archive;
  InternalEntry &m_entry;
};

template <typename InternalEntry = ArchiveInternalEntryTemplate,
          typename Entry = ArchiveEntryTemplate<InternalEntry>>
class ArchiveTemplate : public abstract::Archive {
public:
  [[nodiscard]] std::shared_ptr<abstract::ArchiveEntry> front() const override {
    return front2();
  }

  [[nodiscard]] virtual std::shared_ptr<Entry> front2() const = 0;

  [[nodiscard]] std::shared_ptr<abstract::ArchiveEntry>
  find(const common::Path &path) const override {
    return find2(path);
  }

  [[nodiscard]] virtual std::shared_ptr<Entry>
  find2(const common::Path &path) const = 0;

  std::shared_ptr<abstract::ArchiveEntry>
  insert_file(std::shared_ptr<abstract::ArchiveEntry> at, common::Path path,
              std::shared_ptr<abstract::File> file) override {
    return insert_file2(std::dynamic_pointer_cast<Entry>(at), path,
                        std::move(file));
  }

  virtual std::shared_ptr<Entry>
  insert_file2(std::shared_ptr<Entry> at, common::Path path,
               std::shared_ptr<abstract::File> file) = 0;

  std::shared_ptr<abstract::ArchiveEntry>
  insert_directory(std::shared_ptr<abstract::ArchiveEntry> at,
                   common::Path path) override {
    return insert_directory2(std::dynamic_pointer_cast<Entry>(at),
                             std::move(path));
  }

  virtual std::shared_ptr<Entry> insert_directory2(std::shared_ptr<Entry> at,
                                                   common::Path path) = 0;

  void move(std::shared_ptr<abstract::ArchiveEntry> entry,
            common::Path path) const override {
    move2(std::dynamic_pointer_cast<Entry>(entry), std::move(path));
  }

  virtual void move2(std::shared_ptr<abstract::ArchiveEntry> entry,
                     common::Path path) const = 0;

  void remove(std::shared_ptr<abstract::ArchiveEntry> entry) override {
    remove2(std::dynamic_pointer_cast<Entry>(entry));
  }

  virtual void remove2(std::shared_ptr<Entry> entry) = 0;

private:
};

} // namespace odr::common

#endif // ODR_COMMON_ARCHIVE_H
