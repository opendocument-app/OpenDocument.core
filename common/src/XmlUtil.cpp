#include <access/Storage.h>
#include <access/StorageUtil.h>
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
  return parse(access::StreamUtil::read(in));
}

pugi::xml_document
XmlUtil::parse(const access::ReadStorage &storage, const access::Path &path) {
  if (!storage.isReadable(path))
    throw 123; // TODO
  return parse(access::StorageUtil::read(storage, path));
}

} // namespace common
} // namespace odr
