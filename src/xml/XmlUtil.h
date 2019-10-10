#ifndef ODR_XMLUTIL_H
#define ODR_XMLUTIL_H

#include <functional>

namespace tinyxml2 {
class XMLNode;
class XMLElement;
class XMLAttribute;
class XMLText;
}

namespace odr {

namespace XmlUtil {
typedef std::function<void(const tinyxml2::XMLNode &)> NodeVisiter;
typedef std::function<void(const tinyxml2::XMLElement &)> ElementVisiter;

void visitNodeChildren(const tinyxml2::XMLNode &, NodeVisiter);
void visitElementChildren(const tinyxml2::XMLElement &, ElementVisiter);

void recursiveVisitNodes(const tinyxml2::XMLNode *root, NodeVisiter);
void recursiveVisitElements(const tinyxml2::XMLElement *root, ElementVisiter);
void recursiveVisitElementsWithName(const tinyxml2::XMLElement *root, const char *name, ElementVisiter);
void recursiveVisitElementsWithAttribute(const tinyxml2::XMLElement *root, const char *attribute, ElementVisiter);
}

}

#endif //ODR_XMLUTIL_H
