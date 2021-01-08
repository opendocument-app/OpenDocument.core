#ifndef ODR_ABSTRACT_DOCUMENT_H
#define ODR_ABSTRACT_DOCUMENT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace odr {
enum class DocumentType;
struct DocumentMeta;
class Element;
class TableElement;
template <typename E> class ElementRangeTemplate;
using ElementRange = ElementRangeTemplate<Element>;
} // namespace odr

namespace odr::common {
class Path;
}

namespace odr::abstract {
class DocumentFile;

class Element;
class Slide;
class Sheet;
class Page;
class PageStyle;

class Document {
public:
  virtual ~Document() = default;

  [[nodiscard]] virtual bool editable() const noexcept = 0;
  [[nodiscard]] virtual bool savable(bool encrypted = false) const noexcept = 0;

  [[nodiscard]] virtual DocumentType documentType() const noexcept = 0;
  [[nodiscard]] virtual DocumentMeta documentMeta() const noexcept = 0;

  [[nodiscard]] virtual std::shared_ptr<const Element> root() const = 0;

  virtual void save(const common::Path &path) const = 0;
  virtual void save(const common::Path &path,
                    const std::string &password) const = 0;
};

class TextDocument : public virtual Document {
public:
  [[nodiscard]] virtual std::shared_ptr<PageStyle> pageStyle() const = 0;
};

class Presentation : public virtual Document {
public:
  [[nodiscard]] virtual std::uint32_t slideCount() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const Slide> firstSlide() const = 0;
};

class Spreadsheet : public virtual Document {
public:
  [[nodiscard]] virtual std::uint32_t sheetCount() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const Sheet> firstSheet() const = 0;
};

class Drawing : public virtual Document {
public:
  [[nodiscard]] virtual std::uint32_t pageCount() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const Page> firstPage() const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_DOCUMENT_H
