#include <common/document_style.h>
#include <common/property.h>
#include <ooxml/text/ooxml_text_style.h>

namespace odr::ooxml::text {

namespace {
class OoxmlTextPageStyle final : public common::PageStyle {
public:
  explicit OoxmlTextPageStyle(pugi::xml_node node) : m_node{node} {}

  std::shared_ptr<common::Property> width() const final {
    return std::make_shared<common::ConstProperty>("8.5in");
  }

  std::shared_ptr<common::Property> height() const final {
    return std::make_shared<common::ConstProperty>("11.7in");
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

class OoxmlParagraphStyle final : public common::ParagraphStyle {
public:
  std::shared_ptr<common::Property> textAlign() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> marginTop() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> marginBottom() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> marginLeft() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> marginRight() const final {
    return std::make_shared<common::ConstProperty>();
  }
};

class OoxmlTextStyle final : public common::TextStyle {
public:
  std::shared_ptr<common::Property> fontName() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontSize() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontWeight() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontStyle() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontColor() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> backgroundColor() const final {
    return std::make_shared<common::ConstProperty>();
  }
};
} // namespace

Styles::Styles(pugi::xml_node stylesRoot, pugi::xml_node documentRoot)
    : m_stylesRoot{stylesRoot}, m_documentRoot{documentRoot} {}

std::shared_ptr<common::PageStyle> Styles::pageStyle() const {
  return std::make_shared<OoxmlTextPageStyle>(
      m_documentRoot.child("w:body").child("w:sectPr"));
}

std::shared_ptr<common::ParagraphStyle> Styles::paragraphStyle() const {
  return std::make_shared<OoxmlParagraphStyle>();
}

std::shared_ptr<common::TextStyle> Styles::textStyle() const {
  return std::make_shared<OoxmlTextStyle>();
}

} // namespace odr::ooxml::text
