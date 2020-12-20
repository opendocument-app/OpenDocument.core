#include <access/path.h>
#include <access/storage.h>
#include <access/stream_util.h>
#include <common/xml_util.h>
#include <pugixml.hpp>

namespace odr::common {

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
  auto in = storage.read(path);
  if (!in)
    throw access::FileNotFoundException(path.string());
  const auto success = result.load(*in);
  if (!success)
    throw NotXmlException();
  return result;
}

} // namespace odr::common
