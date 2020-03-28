#ifndef ODR_OPENDOCUMENTTRANSLATOR_H
#define ODR_OPENDOCUMENTTRANSLATOR_H

#include <memory>
#include <string>

namespace odr {
namespace common {
struct TranslationContext;
}
} // namespace odr

namespace odr {
namespace odf {

class OpenDocumentTranslator final {
public:
  OpenDocumentTranslator();
  ~OpenDocumentTranslator();

  bool translate(const std::string &outPath,
                 common::TranslationContext &context) const;
  bool backTranslate(const std::string &diff, const std::string &outPath,
                     common::TranslationContext &context) const;

private:
  class Impl;
  const std::unique_ptr<Impl> impl;
};

} // namespace odf
} // namespace odr

#endif // ODR_OPENDOCUMENTTRANSLATOR_H
