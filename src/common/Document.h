#ifndef ODR_COMMON_DOCUMENT_H
#define ODR_COMMON_DOCUMENT_H

#include <cstdint>
#include <string>

namespace odr {
enum class DocumentType;
struct DocumentMeta;
struct PageProperties;
class Element;
class ElementSiblingRange;
class TableElement;

namespace access {
class Path;
}

namespace common {

class Document {
public:
  virtual ~Document() = default;

  virtual bool editable() const noexcept = 0;
  virtual bool savable(bool encrypted = false) const noexcept = 0;

  virtual DocumentType documentType() const noexcept = 0;
  virtual DocumentMeta documentMeta() const noexcept = 0;

  virtual void save(const access::Path &path) const = 0;
  virtual void save(const access::Path &path,
                    const std::string &password) const = 0;
};

class TextDocument : public virtual Document {
public:
  virtual PageProperties pageProperties() const = 0;

  virtual ElementSiblingRange content() const = 0;
};

class Presentation : public virtual Document {
public:
  virtual ElementSiblingRange slideContent(std::uint32_t index) const = 0;
};

class Spreadsheet : public virtual Document {
public:
  virtual TableElement sheetTable(std::uint32_t index) const = 0;
};

class Graphics : public virtual Document {
public:
  virtual ElementSiblingRange pageContent(std::uint32_t index) const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENT_H
