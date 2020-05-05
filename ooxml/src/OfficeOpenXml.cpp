#include <Context.h>
#include <Crypto.h>
#include <DocumentTranslator.h>
#include <Meta.h>
#include <PresentationTranslator.h>
#include <WorkbookTranslator.h>
#include <access/CfbStorage.h>
#include <access/Path.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Html.h>
#include <common/XmlUtil.h>
#include <fstream>
#include <odr/Config.h>
#include <odr/Exception.h>
#include <odr/Meta.h>
#include <ooxml/OfficeOpenXml.h>
#include <tinyxml2.h>

namespace odr {
namespace ooxml {

namespace {
void generateStyle_(std::ofstream &out, Context &context) {
  // default css
  out << common::Html::odfDefaultStyle();

  switch (context.meta->type) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT: {
    const auto stylesXml =
        common::XmlUtil::parse(*context.storage, "word/styles.xml");
    const tinyxml2::XMLElement *styles = stylesXml->RootElement();
    DocumentTranslator::css(*styles, context);
  } break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    // TODO duplication in generateContent_
    const auto ppt =
        common::XmlUtil::parse(*context.storage, "ppt/presentation.xml");
    const tinyxml2::XMLElement *sizeEle =
        ppt->RootElement()->FirstChildElement("p:sldSz");
    if (sizeEle != nullptr) {
      float widthIn = sizeEle->FindAttribute("cx")->Int64Value() / 914400.0f;
      float heightIn = sizeEle->FindAttribute("cy")->Int64Value() / 914400.0f;

      out << ".slide {";
      out << "width:" << widthIn << "in;";
      out << "height:" << heightIn << "in;";
      out << "}";
    }
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto stylesXml =
        common::XmlUtil::parse(*context.storage, "xl/styles.xml");
    const tinyxml2::XMLElement *styles = stylesXml->RootElement();
    WorkbookTranslator::css(*styles, context);
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

    tinyxml2::XMLElement *body =
        content->FirstChildElement("w:document")->FirstChildElement("w:body");

    DocumentTranslator::html(*body, context);
  } break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    const auto ppt =
        common::XmlUtil::parse(*context.storage, "ppt/presentation.xml");
    const auto pptRelations =
        Meta::parseRelationships(*context.storage, "ppt/presentation.xml");

    common::XmlUtil::recursiveVisitElementsWithName(
        ppt->RootElement(), "p:sldId", [&](const auto &e) {
          const std::string rId = e.FindAttribute("r:id")->Value();

          const auto path = access::Path("ppt").join(pptRelations.at(rId));
          const auto content = common::XmlUtil::parse(*context.storage, path);
          context.relations = Meta::parseRelationships(*context.storage, path);

          if ((context.config->entryOffset > 0) ||
              (context.config->entryCount > 0)) {
            if ((context.entry >= context.config->entryOffset) &&
                (context.entry <
                 context.config->entryOffset + context.config->entryCount)) {
              PresentationTranslator::html(*content->RootElement(), context);
            }
          } else {
            PresentationTranslator::html(*content->RootElement(), context);
          }

          ++context.entry;
        });
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto xls =
        common::XmlUtil::parse(*context.storage, "xl/workbook.xml");
    const auto xlsRelations =
        Meta::parseRelationships(*context.storage, "xl/workbook.xml");

    // TODO this breaks back translation
    context.sharedStringsDocument =
        common::XmlUtil::parse(*context.storage, "xl/sharedStrings.xml");
    if (context.sharedStringsDocument) {
      common::XmlUtil::recursiveVisitElementsWithName(
          context.sharedStringsDocument->RootElement(), "si",
          [&](const auto &child) { context.sharedStrings.push_back(&child); });
    }

    common::XmlUtil::recursiveVisitElementsWithName(
        xls->RootElement(), "sheet", [&](const auto &e) {
          const std::string rId = e.FindAttribute("r:id")->Value();

          const auto path = access::Path("xl").join(xlsRelations.at(rId));
          const auto content = common::XmlUtil::parse(*context.storage, path);
          context.relations = Meta::parseRelationships(*context.storage, path);

          if ((context.config->entryOffset > 0) ||
              (context.config->entryCount > 0)) {
            if ((context.entry >= context.config->entryOffset) &&
                (context.entry <
                 context.config->entryOffset + context.config->entryCount)) {
              WorkbookTranslator::html(*content->RootElement(), context);
            }
          } else {
            WorkbookTranslator::html(*content->RootElement(), context);
          }

          ++context.entry;
        });
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

  bool encrypted() const noexcept { return meta_.encrypted; }

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

  bool translate(const access::Path &path, const Config &config) {
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
  std::unique_ptr<tinyxml2::XMLDocument> style_;
  std::unique_ptr<tinyxml2::XMLDocument> content_;
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

OfficeOpenXml &OfficeOpenXml::operator=(OfficeOpenXml &&) noexcept = default;

OfficeOpenXml::~OfficeOpenXml() = default;

const FileMeta &OfficeOpenXml::meta() const noexcept { return impl_->meta(); }

const access::ReadStorage &OfficeOpenXml::storage() const noexcept {
  return impl_->storage();
}

bool OfficeOpenXml::decrypted() const noexcept { return impl_->decrypted(); }

bool OfficeOpenXml::translatable() const noexcept {
  return impl_->translatable();
}

bool OfficeOpenXml::editable() const noexcept { return impl_->editable(); }

bool OfficeOpenXml::savable(const bool encrypted) const noexcept {
  return impl_->savable(encrypted);
}

bool OfficeOpenXml::decrypt(const std::string &password) {
  return impl_->decrypt(password);
}

void OfficeOpenXml::translate(const access::Path &path, const Config &config) {
  impl_->translate(path, config);
}

void OfficeOpenXml::edit(const std::string &diff) { impl_->edit(diff); }

void OfficeOpenXml::save(const access::Path &path) const { impl_->save(path); }

void OfficeOpenXml::save(const access::Path &path,
                         const std::string &password) const {
  impl_->save(path, password);
}

} // namespace ooxml
} // namespace odr
