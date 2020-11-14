#ifndef ODR_ODF_OPENDOCUMENTTEXT_H
#define ODR_ODF_OPENDOCUMENTTEXT_H

#include <generic/GenericDocument.h>
#include <pugixml.hpp>

namespace odr::odf {

class OpenDocumentText final : public generic::GenericTextDocument {
public:
  OpenDocumentText(pugi::xml_document content, pugi::xml_document styles);

  pugi::xml_document &content();
  pugi::xml_document &styles();

  std::shared_ptr<const generic::GenericElement>
  firstContentElement() const override;

private:
  pugi::xml_document m_content;
  pugi::xml_document m_styles;
};

} // namespace odr::odf

#endif // ODR_ODF_OPENDOCUMENTTEXT_H
