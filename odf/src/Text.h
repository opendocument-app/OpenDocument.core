#ifndef ODR_ODF_OPENDOCUMENTTEXT_H
#define ODR_ODF_OPENDOCUMENTTEXT_H

#include <common/GenericDocument.h>
#include <pugixml.hpp>

namespace odr::odf {

class Text final : public common::GenericTextDocument {
public:
  Text(pugi::xml_document content, pugi::xml_document style);

  pugi::xml_document &content();
  pugi::xml_document &style();

  PageProperties pageProperties() const final;

  std::shared_ptr<const common::GenericElement> firstContentElement() const final;

private:
  pugi::xml_document m_content;
  pugi::xml_document m_style;
};

} // namespace odr::odf

#endif // ODR_ODF_OPENDOCUMENTTEXT_H
