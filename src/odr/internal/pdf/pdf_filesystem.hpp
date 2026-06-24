#pragma once

#include <odr/filesystem.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <map>
#include <memory>

namespace odr::internal::pdf {

class PdfFile;

/// Exposes the low-level object structure of a PDF as a read-only filesystem,
/// so it can be browsed (and rendered) with the generic filesystem HTML viewer
/// instead of needing a dedicated PDF inspector.
///
/// Layout:
///   /trailer              the trailer dictionary
///   /objects/<id>_<gen>   each indirect object's value (the stream dictionary
///                         for stream objects)
///   /streams/<id>_<gen>   the stream bytes of each stream object, decoded
///                         through the `/Filter` chain when possible and the
///                         raw bytes otherwise (e.g. image codecs)
///
/// The object set is taken from the cross-reference table at construction;
/// content is serialized lazily on `open`. The backing `DocumentParser` is held
/// for the lifetime of the filesystem, so treat an instance as transient — one
/// per browse — matching the parser's own usage contract.
class PdfFilesystem final : public abstract::ReadableFilesystem {
public:
  explicit PdfFilesystem(const PdfFile &pdf_file,
                         const Logger &logger = Logger::null());

  [[nodiscard]] bool exists(const AbsPath &path) const override;
  [[nodiscard]] bool is_file(const AbsPath &path) const override;
  [[nodiscard]] bool is_directory(const AbsPath &path) const override;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const AbsPath &path) const override;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const AbsPath &path) const override;

private:
  enum class Kind { directory, trailer, object, stream };
  struct Entry {
    Kind kind{};
    ObjectReference reference;
  };

  // `mutable`: reads memoize in the parser, and the `ReadableFilesystem`
  // interface is `const`.
  mutable DocumentParser m_parser;
  std::map<AbsPath, Entry> m_entries;
};

/// Wraps a PDF's object structure as a browsable `Filesystem`, ready to hand to
/// `html::translate` for the filesystem HTML viewer.
[[nodiscard]] odr::Filesystem
create_object_filesystem(const PdfFile &pdf_file,
                         const Logger &logger = Logger::null());

} // namespace odr::internal::pdf
