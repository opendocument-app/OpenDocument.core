#include <abstract/storage.h>
#include <common/path.h>
#include <odr/exceptions.h>
#include <pugixml.hpp>
#include <util/xml_util.h>

namespace odr::util {

pugi::xml_document xml::parse(const std::string &in) {
  pugi::xml_document result;
  const auto success = result.load_string(in.c_str());
  if (!success)
    throw NoXml();
  return result;
}

pugi::xml_document xml::parse(std::istream &in) {
  pugi::xml_document result;
  const auto success = result.load(in);
  if (!success)
    throw NoXml();
  return result;
}

pugi::xml_document xml::parse(const abstract::ReadStorage &storage,
                              const common::Path &path) {
  pugi::xml_document result;
  auto in = storage.read(path);
  if (!in)
    throw FileNotFound();
  const auto success = result.load(*in);
  if (!success)
    throw NoXml();
  return result;
}

} // namespace odr::util
