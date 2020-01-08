#include "XmlUtil.h"
#include "tinyxml2.h"
#include "../io/Storage.h"
#include "../io/StreamUtil.h"
#include "../io/StorageUtil.h"

namespace odr {

std::unique_ptr<tinyxml2::XMLDocument> XmlUtil::parse(const std::string &in) {
    auto result = std::make_unique<tinyxml2::XMLDocument>();
    tinyxml2::XMLError error = result->Parse(in.data(), in.size());
    if (error != tinyxml2::XML_SUCCESS) throw NotXmlException();
    return result;
}

std::unique_ptr<tinyxml2::XMLDocument> XmlUtil::parse(Source &in) {
    return parse(StreamUtil::read(in));
}

std::unique_ptr<tinyxml2::XMLDocument> XmlUtil::parse(const Storage &storage, const Path &path) {
    return parse(StorageUtil::read(storage, path));
}

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

void XmlUtil::recursiveVisitElements(const tinyxml2::XMLElement *root, ElementVisiter visiter) {
    if (root == nullptr) return;
    visiter(*root);
    visitElementChildren(*root, [&](const auto &child) {
        recursiveVisitElements(&child, visiter);
    });
}

void XmlUtil::recursiveVisitElementsWithName(const tinyxml2::XMLElement *root, const char *name, ElementVisiter visiter) {
    recursiveVisitElements(root, [&](const tinyxml2::XMLElement &element) {
        if (std::strcmp(element.Name(), name) != 0) return;
        visiter(element);
    });
}

void XmlUtil::recursiveVisitElementsWithAttribute(const tinyxml2::XMLElement *root, const char *attribute, ElementVisiter visiter) {
    recursiveVisitElements(root, [&](const tinyxml2::XMLElement &element) {
        if (element.FindAttribute(attribute) == nullptr) return;
        visiter(element);
    });
}

}
