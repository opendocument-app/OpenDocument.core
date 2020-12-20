#ifndef ODR_ODF_META_H
#define ODR_ODF_META_H

#include <access/path.h>
#include <cstdint>
#include <exception>
#include <string>
#include <unordered_map>

namespace pugi {
class xml_document;
}

namespace odr {
struct FileMeta;
struct DocumentMeta;

namespace access {
class ReadStorage;
} // namespace access
} // namespace odr

namespace odr::odf {

struct NoOpenDocumentFileException final : public std::exception {
  const char *what() const noexcept final {
    return "not an open document file";
  }
};

FileMeta parseFileMeta(const access::ReadStorage &storage,
                       const pugi::xml_document *manifest);
DocumentMeta parseDocumentMeta(const pugi::xml_document *meta,
                               const pugi::xml_document &content);

} // namespace odr::odf

#endif // ODR_ODF_META_H
