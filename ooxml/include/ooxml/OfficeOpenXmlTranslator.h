#ifndef ODR_OFFICEOPENXMLTRANSLATOR_H
#define ODR_OFFICEOPENXMLTRANSLATOR_H

#include <memory>
#include <string>

namespace odr {
namespace common {
struct TranslationContext;
}
} // namespace odr

namespace odr {
namespace ooxml {

class OfficeOpenXmlTranslator final {
public:
  OfficeOpenXmlTranslator();
  ~OfficeOpenXmlTranslator();

  bool translate(const std::string &outPath,
                 common::TranslationContext &context) const;
  bool backTranslate(const std::string &diff, const std::string &outPath,
                     common::TranslationContext &context) const;

private:
  class Impl;
  const std::unique_ptr<Impl> impl;
};

} // namespace ooxml
} // namespace odr

#endif // ODR_OFFICEOPENXMLTRANSLATOR_H
