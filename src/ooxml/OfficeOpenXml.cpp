#include <ooxml/Context.h>
#include <ooxml/Crypto.h>
#include <ooxml/DocumentTranslator.h>
#include <ooxml/Meta.h>
#include <ooxml/PresentationTranslator.h>
#include <ooxml/WorkbookTranslator.h>
#include <access/CfbStorage.h>
#include <access/Path.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Html.h>
#include <common/XmlUtil.h>
#include <fstream>
#include <odr/Html.h>
#include <odr/Exception.h>
#include <odr/File.h>
#include <ooxml/OfficeOpenXml.h>
#include <pugixml.hpp>

namespace odr::ooxml {

namespace {
void generateStyle_(std::ofstream &out, Context &context) {
  // default css
  out << common::Html::odfDefaultStyle();

  switch (context.meta->type) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT: {
    const auto styles =
        common::XmlUtil::parse(*context.storage, "word/styles.xml");
    DocumentTranslator::css(styles.document_element(), context);
  } break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    // TODO that should go to `PresentationTranslator::css`

    // TODO duplication in generateContent_
    const auto ppt =
        common::XmlUtil::parse(*context.storage, "ppt/presentation.xml");
    const auto sizeEle = ppt.select_node("//p:sldSz").node();
    if (!sizeEle)
      break;
    const float widthIn = sizeEle.attribute("cx").as_float() / 914400.0f;
    const float heightIn = sizeEle.attribute("cy").as_float() / 914400.0f;

    out << ".slide {";
    out << "width:" << widthIn << "in;";
    out << "height:" << heightIn << "in;";
    out << "}";
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto styles =
        common::XmlUtil::parse(*context.storage, "xl/styles.xml");
    WorkbookTranslator::css(styles.document_element(), context);
  } break;
  default:
    throw std::invalid_argument("file.getMeta().type");
  }
}

void generateScript_(std::ofstream &out, Context &) {
  out << common::Html::defaultScript();
}

void generateContent_(Context &context) {
  context.entry = 0;

  switch (context.meta->type) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT: {
    const auto content =
        common::XmlUtil::parse(*context.storage, "word/document.xml");
    context.relations =
        Meta::parseRelationships(*context.storage, "word/document.xml");

    const auto body = content.child("w:document").child("w:body");
    DocumentTranslator::html(body, context);
  } break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    const auto ppt =
        common::XmlUtil::parse(*context.storage, "ppt/presentation.xml");
    const auto pptRelations =
        Meta::parseRelationships(*context.storage, "ppt/presentation.xml");

    for (auto &&e : ppt.select_nodes("//p:sldId")) {
      const std::string rId = e.node().attribute("r:id").as_string();

      const auto path = access::Path("ppt").join(pptRelations.at(rId));
      const auto content = common::XmlUtil::parse(*context.storage, path);
      context.relations = Meta::parseRelationships(*context.storage, path);

      if ((context.config->entryOffset > 0) ||
          (context.config->entryCount > 0)) {
        if ((context.entry >= context.config->entryOffset) &&
            (context.entry <
             context.config->entryOffset + context.config->entryCount)) {
          PresentationTranslator::html(content, context);
        }
      } else {
        PresentationTranslator::html(content, context);
      }

      ++context.entry;
    }
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto xls =
        common::XmlUtil::parse(*context.storage, "xl/workbook.xml");
    const auto xlsRelations =
        Meta::parseRelationships(*context.storage, "xl/workbook.xml");

    // TODO this breaks back translation
    if (context.storage->isFile("xl/sharedStrings.xml")) {
      context.sharedStringsDocument =
          common::XmlUtil::parse(*context.storage, "xl/sharedStrings.xml");
      for (auto &&e : context.sharedStringsDocument.select_nodes("//si")) {
        context.sharedStrings.push_back(e.node());
      }
    }

    for (auto &&e : xls.select_nodes("//sheet")) {
      const std::string rId = e.node().attribute("r:id").as_string();

      const auto path = access::Path("xl").join(xlsRelations.at(rId));
      const auto content = common::XmlUtil::parse(*context.storage, path);
      context.relations = Meta::parseRelationships(*context.storage, path);

      if ((context.config->entryOffset > 0) ||
          (context.config->entryCount > 0)) {
        if ((context.entry >= context.config->entryOffset) &&
            (context.entry <
             context.config->entryOffset + context.config->entryCount)) {
          WorkbookTranslator::html(content, context);
        }
      } else {
        WorkbookTranslator::html(content, context);
      }

      ++context.entry;
    }
  } break;
  default:
    throw std::invalid_argument("file.getMeta().type");
  }
}
} // namespace

class OfficeOpenXml::Impl {
public:
  explicit Impl(const char *path) : Impl(access::Path(path)) {}

  explicit Impl(const std::string &path) : Impl(access::Path(path)) {}

  explicit Impl(const access::Path &path)
      : Impl(
            std::unique_ptr<access::ReadStorage>(new access::ZipReader(path))) {
    try {
      storage_ = std::make_unique<access::ZipReader>(path);
      meta_ = Meta::parseFileMeta(*storage_);
      return;
    } catch (access::NoZipFileException &) {
    }

    try {
      storage_ = std::make_unique<access::CfbReader>(path);
      meta_ = Meta::parseFileMeta(*storage_);
      return;
    } catch (access::NoCfbFileException &) {
    }

    throw UnknownFileType();
  }

  explicit Impl(std::unique_ptr<access::ReadStorage> &&storage) {
    meta_ = Meta::parseFileMeta(*storage);
    storage_ = std::move(storage);
  }

  explicit Impl(std::unique_ptr<access::ReadStorage> &storage) {
    meta_ = Meta::parseFileMeta(*storage);
    storage_ = std::move(storage);
  }

  FileType type() const noexcept { return meta_.type; }

  const FileMeta &meta() const noexcept { return meta_; }

  const access::ReadStorage &storage() const noexcept { return *storage_; }

  bool decrypted() const noexcept { return decrypted_; }

  bool translatable() const noexcept { return true; }

  bool editable() const noexcept { return false; }

  bool savable(const bool) const noexcept { return false; }

  bool decrypt(const std::string &password) {
    // TODO throw if not encrypted
    // TODO throw if decrypted
    const std::string encryptionInfo =
        access::StreamUtil::read(*storage_->read("EncryptionInfo"));
    // TODO cache Crypto::Util
    Crypto::Util util(encryptionInfo);
    const std::string key = util.deriveKey(password);
    if (!util.verify(key))
      return false;
    const std::string encryptedPackage =
        access::StreamUtil::read(*storage_->read("EncryptedPackage"));
    const std::string decryptedPackage = util.decrypt(encryptedPackage, key);
    storage_ = std::make_unique<access::ZipReader>(decryptedPackage, false);
    meta_ = Meta::parseFileMeta(*storage_);
    decrypted_ = true;
    return true;
  }

  bool translate(const access::Path &path, const Html::Config &config) {
    // TODO throw if not decrypted
    std::ofstream out(path);
    if (!out.is_open())
      return false;
    context_.config = &config;
    context_.meta = &meta_;
    context_.storage = storage_.get();
    context_.output = &out;

    out << common::Html::doctype();
    out << "<html><head>";
    out << common::Html::defaultHeaders();
    out << "<style>";
    generateStyle_(out, context_);
    out << "</style>";
    out << "</head>";

    out << "<body " << common::Html::bodyAttributes(config) << ">";
    generateContent_(context_);
    out << "</body>";

    out << "<script>";
    generateScript_(out, context_);
    out << "</script>";
    out << "</html>";

    context_.config = nullptr;
    context_.output = nullptr;
    out.close();
    return true;
  }

  bool edit(const std::string &) { return false; }

  bool save(const access::Path &) const { return false; }

  bool save(const access::Path &, const std::string &) const { return false; }

private:
  std::unique_ptr<access::ReadStorage> storage_;

  FileMeta meta_;

  bool decrypted_{false};

  Context context_;
  pugi::xml_document style_;
  pugi::xml_document content_;
};

OfficeOpenXml::OfficeOpenXml(const char *path)
    : impl_(std::make_unique<Impl>(path)) {}

OfficeOpenXml::OfficeOpenXml(const std::string &path)
    : impl_(std::make_unique<Impl>(path)) {}

OfficeOpenXml::OfficeOpenXml(const access::Path &path)
    : impl_(std::make_unique<Impl>(path)) {}

OfficeOpenXml::OfficeOpenXml(std::unique_ptr<access::ReadStorage> &&storage)
    : impl_(std::make_unique<Impl>(storage)) {}

OfficeOpenXml::OfficeOpenXml(std::unique_ptr<access::ReadStorage> &storage)
    : impl_(std::make_unique<Impl>(storage)) {}

OfficeOpenXml::OfficeOpenXml(OfficeOpenXml &&) noexcept = default;

OfficeOpenXml::~OfficeOpenXml() = default;

OfficeOpenXml &OfficeOpenXml::operator=(OfficeOpenXml &&) noexcept = default;

const FileMeta &OfficeOpenXml::meta() const noexcept { return impl_->meta(); }

const access::ReadStorage &OfficeOpenXml::storage() const noexcept {
  return impl_->storage();
}

bool OfficeOpenXml::editable() const noexcept { return impl_->editable(); }

bool OfficeOpenXml::savable(const bool encrypted) const noexcept {
  return impl_->savable(encrypted);
}

void OfficeOpenXml::save(const access::Path &path) const { impl_->save(path); }

void OfficeOpenXml::save(const access::Path &path,
                         const std::string &password) const {
  impl_->save(path, password);
}

} // namespace odr::ooxml
