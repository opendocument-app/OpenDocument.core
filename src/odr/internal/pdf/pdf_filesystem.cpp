#include <odr/internal/pdf/pdf_filesystem.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/pdf/pdf_file_object.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace odr::internal::pdf {

namespace {

std::string object_name(const ObjectReference &reference) {
  return std::to_string(reference.id) + "_" + std::to_string(reference.gen);
}

// Walks a precomputed set of paths. Mirrors the virtual-filesystem walker: the
// entry map carries only path + kind (file vs directory); content is produced
// by `open`, so the walk itself never touches the parser.
class PdfFileWalker final : public abstract::FileWalker {
public:
  using Entries = std::map<AbsPath, bool>; // path -> is_file

  PdfFileWalker(const AbsPath &root, const Entries &entries) {
    for (const auto &[path, is_file] : entries) {
      if (path.ancestor_of(root)) {
        m_entries[path] = is_file;
      }
    }
    m_iterator = std::begin(m_entries);
  }

  [[nodiscard]] std::unique_ptr<FileWalker> clone() const override {
    return std::make_unique<PdfFileWalker>(*this);
  }

  [[nodiscard]] bool equals(const FileWalker &rhs_) const override {
    const auto &rhs = dynamic_cast<const PdfFileWalker &>(rhs_);
    return m_iterator == rhs.m_iterator;
  }

  [[nodiscard]] bool end() const override {
    return m_iterator == std::end(m_entries);
  }

  [[nodiscard]] std::uint32_t depth() const override { return 0; }

  [[nodiscard]] AbsPath path() const override { return m_iterator->first; }

  [[nodiscard]] bool is_file() const override { return m_iterator->second; }

  [[nodiscard]] bool is_directory() const override { return !is_file(); }

  void pop() override {}

  void next() override { ++m_iterator; }

  void flat_next() override {}

private:
  Entries m_entries;
  Entries::iterator m_iterator;
};

} // namespace

PdfFilesystem::PdfFilesystem(const PdfFile &pdf_file, const Logger &logger)
    : m_parser{pdf_file.create_parser(logger)} {
  m_entries[AbsPath("/")] = {Kind::directory, {}};
  m_entries[AbsPath("/objects")] = {Kind::directory, {}};
  m_entries[AbsPath("/trailer")] = {Kind::trailer, {}};

  bool any_stream = false;
  for (const auto &[reference, entry] : m_parser.xref().table) {
    if (entry.is_free()) {
      continue;
    }

    bool has_stream = false;
    try {
      has_stream = m_parser.read_object(reference).has_stream;
    } catch (...) {
      continue; // unreadable object: leave it out of the listing
    }

    const RelPath name(object_name(reference));
    m_entries[AbsPath("/objects").join(name)] = {Kind::object, reference};
    if (has_stream) {
      m_entries[AbsPath("/streams").join(name)] = {Kind::stream, reference};
      any_stream = true;
    }
  }

  if (any_stream) {
    m_entries[AbsPath("/streams")] = {Kind::directory, {}};
  }
}

bool PdfFilesystem::exists(const AbsPath &path) const {
  return m_entries.contains(path);
}

bool PdfFilesystem::is_file(const AbsPath &path) const {
  const auto it = m_entries.find(path);
  return it != std::end(m_entries) && it->second.kind != Kind::directory;
}

bool PdfFilesystem::is_directory(const AbsPath &path) const {
  const auto it = m_entries.find(path);
  return it != std::end(m_entries) && it->second.kind == Kind::directory;
}

std::unique_ptr<abstract::FileWalker>
PdfFilesystem::file_walker(const AbsPath &path) const {
  PdfFileWalker::Entries entries;
  for (const auto &[entry_path, entry] : m_entries) {
    entries[entry_path] = entry.kind != Kind::directory;
  }
  return std::make_unique<PdfFileWalker>(path, entries);
}

std::shared_ptr<abstract::File> PdfFilesystem::open(const AbsPath &path) const {
  const auto it = m_entries.find(path);
  if (it == std::end(m_entries)) {
    return {};
  }

  std::string content;
  switch (const Entry &entry = it->second; entry.kind) {
  case Kind::directory:
    return {};
  case Kind::trailer:
    content = m_parser.trailer().to_string();
    break;
  case Kind::object:
    content = m_parser.read_object(entry.reference).object.to_string();
    break;
  case Kind::stream:
    try {
      content = m_parser.read_decoded_stream(entry.reference);
    } catch (...) {
      // image codecs and unsupported filters cannot be decoded: fall back to
      // the raw stream bytes so the object is still inspectable.
      content = m_parser.read_object_stream(entry.reference);
    }
    break;
  }

  return std::make_shared<MemoryFile>(std::move(content));
}

odr::Filesystem create_object_filesystem(const PdfFile &pdf_file,
                                         const Logger &logger) {
  return odr::Filesystem(std::make_shared<PdfFilesystem>(pdf_file, logger));
}

} // namespace odr::internal::pdf
