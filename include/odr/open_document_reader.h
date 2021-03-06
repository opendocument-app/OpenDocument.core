#ifndef ODR_OPEN_DOCUMENT_READER_H
#define ODR_OPEN_DOCUMENT_READER_H

#include <string>

namespace odr {

class OpenDocumentReader {
public:
  [[nodiscard]] static std::string version() noexcept;
  [[nodiscard]] static std::string commit() noexcept;

private:
  OpenDocumentReader() = default;
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_H
