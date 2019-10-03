#include "XmlUtil.h"
#include "tinyxml2.h"

namespace odr {

static inline void visitIfElement(const tinyxml2::XMLNode &node, XmlUtil::ElementVisiter visiter) {
    const tinyxml2::XMLElement *element = node.ToElement();
    if (element == nullptr) return;
    visiter(*element);
}

void XmlUtil::visitNodeChildren(const tinyxml2::XMLNode &node, NodeVisiter visiter) {
    for (auto child = node.FirstChild(); child != nullptr; child = child->NextSibling()) {
        visiter(*child);
    }
}

void XmlUtil::visitElementChildren(const tinyxml2::XMLElement &element, ElementVisiter visiter) {
    visitNodeChildren(element, [&](const auto &child) {
        visitIfElement(child, visiter);
    });
}

void XmlUtil::recursiveVisitNodes(const tinyxml2::XMLNode *root, NodeVisiter visiter) {
    if (root == nullptr) return;
    visiter(*root);
    visitNodeChildren(*root, [&](const auto &child) {
        recursiveVisitNodes(&child, visiter);
    });
}

void XmlUtil::recursiveVisitElementsWithName(const tinyxml2::XMLElement *root, const char *name, ElementVisiter visiter) {
    recursiveVisitNodes(root, [&](const tinyxml2::XMLNode &node) {
        visitIfElement(node, [&](const auto &element) {
            if (std::strcmp(element.Name(), name) != 0) return;
            visiter(element);
        });
    });
}

void XmlUtil::recursiveVisitElementsWithAttribute(const tinyxml2::XMLElement *root, const char *attribute, ElementVisiter visiter) {
    recursiveVisitNodes(root, [&](const tinyxml2::XMLNode &node) {
        visitIfElement(node, [&](const auto &element) {
            if (element.FindAttribute(attribute) == nullptr) return;
            visiter(element);
        });
    });
}

}
