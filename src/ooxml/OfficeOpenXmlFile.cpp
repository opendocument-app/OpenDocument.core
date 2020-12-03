#include <ooxml/OfficeOpenXmlFile.h>
#include <ooxml/Meta.h>

namespace odr::ooxml {

OfficeOpenXmlFile::OfficeOpenXmlFile(std::shared_ptr<access::ReadStorage> storage) {
  m_meta = Meta::parseFileMeta(*storage);
  m_storage = std::move(storage);
}

FileType OfficeOpenXmlFile::fileType() const noexcept {
  return m_meta.type;
}

FileMeta OfficeOpenXmlFile::fileMeta() const noexcept {
  return m_meta;
}

}
