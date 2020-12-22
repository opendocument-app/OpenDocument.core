#include <common/document_style.h>
#include <common/property.h>
#include <ooxml/text/ooxml_text_style.h>

namespace odr::ooxml::text {

namespace {
class OoxmlTextPageStyle final : public common::PageStyle {
public:
  explicit OoxmlTextPageStyle(pugi::xml_node node) : m_node{node} {}

  std::shared_ptr<common::Property> width() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> height() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> marginTop() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> marginBottom() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> marginLeft() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> marginRight() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> printOrientation() const final {
    return std::make_shared<common::ConstProperty>("");
  }

private:
  pugi::xml_node m_node;
};
} // namespace

Styles::Styles(pugi::xml_node stylesRoot, pugi::xml_node documentRoot)
    : m_stylesRoot{stylesRoot}, m_documentRoot{documentRoot} {}

std::shared_ptr<common::PageStyle> Styles::pageStyle() const {
  return std::make_shared<OoxmlTextPageStyle>(
      m_documentRoot.child("w:body").child("w:sectPr"));
}

} // namespace odr::ooxml::text
