#ifndef ODR_COMMON_DOCUMENT_H
#define ODR_COMMON_DOCUMENT_H

#include <memory>
#include <common/File.h>

namespace odr {
struct Config;
struct FileMeta;
struct PageProperties;

namespace access {
class Path;
}

namespace common {
class Element;
class Table;

class Document : public File {
public:
  virtual bool decrypted() const noexcept = 0;
  virtual bool translatable() const noexcept = 0;
  virtual bool editable() const noexcept = 0;
  virtual bool savable(bool encrypted) const noexcept = 0;

  virtual bool decrypt(const std::string &password) = 0;

  virtual void translate(const access::Path &path, const Config &config) = 0;
  virtual void edit(const std::string &diff) = 0;

  virtual void save(const access::Path &path) const = 0;
  virtual void save(const access::Path &path,
                    const std::string &password) const = 0;
};

class TextDocument : public Document {
public:
  virtual PageProperties pageProperties() const = 0;

  virtual std::shared_ptr<const Element> firstContentElement() const = 0;
};

class Presentation : public Document {
public:
  virtual std::shared_ptr<const Element>
  firstSlideContentElement(std::uint32_t index) const = 0;
};

class Spreadsheet : public Document {
public:
  virtual std::shared_ptr<const Table> table(std::uint32_t index) const = 0;
};

class Graphics : public Document {
public:
  virtual std::shared_ptr<const Element>
  firstPageContentElement(std::uint32_t index) const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENT_H
