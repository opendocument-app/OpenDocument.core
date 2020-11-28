#ifndef ODR_COMMON_DOCUMENT_H
#define ODR_COMMON_DOCUMENT_H

#include <memory>
#include <common/File.h>

namespace odr {
struct FileMeta;
struct PageProperties;
class Element;
class ElementSiblingRange;
class Table;

namespace access {
class Path;
}

namespace common {

class Document : public File {
public:
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
  virtual Table sheetTable(std::uint32_t index) const = 0;
};

class Graphics : public virtual Document {
public:
  virtual ElementSiblingRange pageContent(std::uint32_t index) const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENT_H
