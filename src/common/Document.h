#ifndef ODR_COMMON_DOCUMENT_H
#define ODR_COMMON_DOCUMENT_H

#include <cstdint>
#include <string>
#include <vector>

namespace odr {
enum class DocumentType;
struct DocumentMeta;
struct PageProperties;
class Element;
class TableElement;
template <typename E> class ElementRangeTemplate;
using ElementRange = ElementRangeTemplate<Element>;
struct Slide;
struct Sheet;
struct Page;

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

  virtual ElementRange content() const = 0;
};

class Presentation : public virtual Document {
public:
  virtual std::uint32_t slideCount() const = 0;
  virtual std::vector<Slide> slides() const = 0;
};

class Spreadsheet : public virtual Document {
public:
  virtual std::uint32_t sheetCount() const = 0;
  virtual std::vector<Sheet> sheets() const = 0;
};

class Graphics : public virtual Document {
public:
  virtual std::uint32_t pageCount() const = 0;
  virtual std::vector<Page> pages() const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENT_H
