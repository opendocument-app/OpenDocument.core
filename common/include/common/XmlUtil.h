#ifndef ODR_XMLUTIL_H
#define ODR_XMLUTIL_H

#include <functional>
#include <memory>

namespace tinyxml2 {
class XMLDocument;
class XMLNode;
class XMLElement;
class XMLAttribute;
class XMLText;
}

namespace odr {

class Source;
class Path;
class Storage;

class NotXmlException : public std::exception {
public:
    const char *what() const noexcept override { return "not xml"; }
};

namespace XmlUtil {
typedef std::function<void(const tinyxml2::XMLNode &)> NodeVisiter;
typedef std::function<void(const tinyxml2::XMLElement &)> ElementVisiter;
typedef std::function<void(const tinyxml2::XMLAttribute &)> AttributeVisiter;

extern std::unique_ptr<tinyxml2::XMLDocument> parse(const std::string &);
extern std::unique_ptr<tinyxml2::XMLDocument> parse(Source &);
extern std::unique_ptr<tinyxml2::XMLDocument> parse(const Storage &, const Path &);

extern const tinyxml2::XMLElement *firstChildElement(const tinyxml2::XMLElement &);

extern void visitNodeChildren(const tinyxml2::XMLNode &, NodeVisiter);
extern void visitElementChildren(const tinyxml2::XMLElement &, ElementVisiter);
extern void visitElementAttributes(const tinyxml2::XMLElement &, AttributeVisiter);

extern void recursiveVisitNodes(const tinyxml2::XMLNode *root, NodeVisiter);
extern void recursiveVisitElements(const tinyxml2::XMLElement *root, ElementVisiter);
extern void recursiveVisitElementsWithName(const tinyxml2::XMLElement *root, const char *name, ElementVisiter);
extern void recursiveVisitElementsWithAttribute(const tinyxml2::XMLElement *root, const char *attribute, ElementVisiter);
}

}

#endif //ODR_XMLUTIL_H
