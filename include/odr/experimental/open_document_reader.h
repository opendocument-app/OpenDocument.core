#ifndef ODR_EXPERIMENTAL_OPEN_DOCUMENT_READER_H
#define ODR_EXPERIMENTAL_OPEN_DOCUMENT_READER_H

#include <string>

namespace odr::experimental {

class OpenDocumentReader {
public:
  [[nodiscard]] static std::string version() noexcept;
  [[nodiscard]] static std::string commit() noexcept;

private:
  OpenDocumentReader() = default;
};

} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_OPEN_DOCUMENT_READER_H
