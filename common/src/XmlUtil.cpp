#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/XmlUtil.h>
#include <pugixml.hpp>

namespace odr {
namespace common {

pugi::xml_document XmlUtil::parse(const std::string &in) {
  pugi::xml_document result;
  const auto success = result.load_string(in.c_str());
  if (!success)
    throw NotXmlException();
  return result;
}

pugi::xml_document XmlUtil::parse(std::istream &in) {
  pugi::xml_document result;
  const auto success = result.load(in);
  if (!success)
    throw NotXmlException();
  return result;
}

pugi::xml_document XmlUtil::parse(const access::ReadStorage &storage,
                                  const access::Path &path) {
  pugi::xml_document result;
  const auto success = result.load(*storage.read(path));
  if (!success)
    throw NotXmlException();
  return result;
}

} // namespace common
} // namespace odr
