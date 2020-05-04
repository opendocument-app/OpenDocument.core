#ifndef ODR_COMMON_XML_UTIL_H
#define ODR_COMMON_XML_UTIL_H

#include <functional>
#include <memory>

namespace tinyxml2 {
class XMLDocument;
class XMLNode;
class XMLElement;
class XMLAttribute;
class XMLText;
} // namespace tinyxml2

namespace odr {
namespace access {
class Path;
class ReadStorage;
} // namespace access
} // namespace odr

namespace odr {
namespace common {

struct NotXmlException final : public std::exception {
  const char *what() const noexcept override { return "not xml"; }
};

namespace XmlUtil {
typedef std::function<void(const tinyxml2::XMLNode &)> NodeVisiter;
typedef std::function<void(const tinyxml2::XMLElement &)> ElementVisiter;
typedef std::function<void(const tinyxml2::XMLAttribute &)> AttributeVisiter;

std::unique_ptr<tinyxml2::XMLDocument> parse(const std::string &);
std::unique_ptr<tinyxml2::XMLDocument> parse(std::istream &);
std::unique_ptr<tinyxml2::XMLDocument> parse(const access::ReadStorage &,
                                             const access::Path &);

const tinyxml2::XMLElement *firstChildElement(const tinyxml2::XMLElement &);

void visitNodeChildren(const tinyxml2::XMLNode &, NodeVisiter);
void visitElementChildren(const tinyxml2::XMLElement &, ElementVisiter);
void visitElementAttributes(const tinyxml2::XMLElement &, AttributeVisiter);

void recursiveVisitNodes(const tinyxml2::XMLNode *root, NodeVisiter);
void recursiveVisitElements(const tinyxml2::XMLElement *root, ElementVisiter);
void recursiveVisitElementsWithName(const tinyxml2::XMLElement *root,
                                    const char *name, ElementVisiter);
void recursiveVisitElementsWithAttribute(const tinyxml2::XMLElement *root,
                                         const char *attribute, ElementVisiter);
} // namespace XmlUtil

} // namespace common
} // namespace odr

#endif // ODR_COMMON_XML_UTIL_H
