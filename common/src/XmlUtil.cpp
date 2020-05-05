#include <access/Storage.h>
#include <access/StorageUtil.h>
#include <access/StreamUtil.h>
#include <common/XmlUtil.h>
#include <tinyxml2.h>

namespace odr {
namespace common {

namespace {
inline void visitIfElement_(const tinyxml2::XMLNode &node,
                            XmlUtil::ElementVisiter visiter) {
  const tinyxml2::XMLElement *element = node.ToElement();
  if (element == nullptr)
    return;
  visiter(*element);
}
} // namespace

std::unique_ptr<tinyxml2::XMLDocument> XmlUtil::parse(const std::string &in) {
  auto result = std::make_unique<tinyxml2::XMLDocument>();
  tinyxml2::XMLError error = result->Parse(in.data(), in.size());
  if (error != tinyxml2::XML_SUCCESS)
    throw NotXmlException();
  return result;
}

std::unique_ptr<tinyxml2::XMLDocument> XmlUtil::parse(std::istream &in) {
  return parse(access::StreamUtil::read(in));
}

std::unique_ptr<tinyxml2::XMLDocument>
XmlUtil::parse(const access::ReadStorage &storage, const access::Path &path) {
  if (!storage.isReadable(path))
    return nullptr;
  return parse(access::StorageUtil::read(storage, path));
}

const tinyxml2::XMLElement *
XmlUtil::firstChildElement(const tinyxml2::XMLElement &root) {
  const tinyxml2::XMLNode *child = root.FirstChild();
  while (child != nullptr) {
    if (child->ToElement() != nullptr)
      return child->ToElement();
    child = child->NextSibling();
  }
  return nullptr;
}

void XmlUtil::visitNodeChildren(const tinyxml2::XMLNode &node,
                                NodeVisiter visiter) {
  for (auto child = node.FirstChild(); child != nullptr;
       child = child->NextSibling()) {
    visiter(*child);
  }
}

void XmlUtil::visitElementChildren(const tinyxml2::XMLElement &element,
                                   ElementVisiter visiter) {
  visitNodeChildren(
      element, [&](const auto &child) { visitIfElement_(child, visiter); });
}

void XmlUtil::visitElementAttributes(const tinyxml2::XMLElement &element,
                                     AttributeVisiter visiter) {
  for (auto attr = element.FirstAttribute(); attr != nullptr;
       attr = attr->Next()) {
    visiter(*attr);
  }
}

void XmlUtil::recursiveVisitNodes(const tinyxml2::XMLNode *root,
                                  NodeVisiter visiter) {
  if (root == nullptr)
    return;
  visiter(*root);
  visitNodeChildren(
      *root, [&](const auto &child) { recursiveVisitNodes(&child, visiter); });
}

void XmlUtil::recursiveVisitElements(const tinyxml2::XMLElement *root,
                                     ElementVisiter visiter) {
  if (root == nullptr)
    return;
  visiter(*root);
  visitElementChildren(*root, [&](const auto &child) {
    recursiveVisitElements(&child, visiter);
  });
}

void XmlUtil::recursiveVisitElementsWithName(const tinyxml2::XMLElement *root,
                                             const char *name,
                                             ElementVisiter visiter) {
  recursiveVisitElements(root, [&](const tinyxml2::XMLElement &element) {
    if (std::strcmp(element.Name(), name) != 0)
      return;
    visiter(element);
  });
}

void XmlUtil::recursiveVisitElementsWithAttribute(
    const tinyxml2::XMLElement *root, const char *attribute,
    ElementVisiter visiter) {
  recursiveVisitElements(root, [&](const tinyxml2::XMLElement &element) {
    if (element.FindAttribute(attribute) == nullptr)
      return;
    visiter(element);
  });
}

} // namespace common
} // namespace odr
