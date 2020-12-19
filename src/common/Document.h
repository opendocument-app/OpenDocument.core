#ifndef ODR_COMMON_DOCUMENT_H
#define ODR_COMMON_DOCUMENT_H

#include <cstdint>
#include <memory>
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

namespace access {
class Path;
}

namespace common {
class Element;
class Slide;
class Sheet;
class Page;

class Document {
public:
  virtual ~Document() = default;

  virtual bool editable() const noexcept = 0;
  virtual bool savable(bool encrypted = false) const noexcept = 0;

  virtual DocumentType documentType() const noexcept = 0;
  virtual DocumentMeta documentMeta() const noexcept = 0;

  virtual std::shared_ptr<const Element> root() const = 0;

  virtual void save(const access::Path &path) const = 0;
  virtual void save(const access::Path &path,
                    const std::string &password) const = 0;
};

class TextDocument : public virtual Document {
public:
  virtual PageProperties pageProperties() const = 0;
};

class Presentation : public virtual Document {
public:
  virtual std::uint32_t slideCount() const = 0;

  virtual std::shared_ptr<const Slide> firstSlide() const = 0;
};

class Spreadsheet : public virtual Document {
public:
  virtual std::uint32_t sheetCount() const = 0;

  virtual std::shared_ptr<const Sheet> firstSheet() const = 0;
};

class Drawing : public virtual Document {
public:
  virtual std::uint32_t pageCount() const = 0;

  virtual std::shared_ptr<const Page> firstPage() const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENT_H
