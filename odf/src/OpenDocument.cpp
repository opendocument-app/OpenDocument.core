#include <ContentTranslator.h>
#include <Context.h>
#include <Crypto.h>
#include <Meta.h>
#include <StyleTranslator.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Html.h>
#include <common/XmlUtil.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <odf/OpenDocument.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <tinyxml2.h>

namespace odr {
namespace odf {

namespace {
void generateStyle_(std::ofstream &out, Context &context) {
  out << common::Html::odfDefaultStyle();

  if (context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) {
    out << common::Html::odfSpreadsheetDefaultStyle();
  }

  const auto stylesXml = common::XmlUtil::parse(*context.storage, "styles.xml");
  tinyxml2::XMLHandle stylesHandle(stylesXml.get());

  const tinyxml2::XMLElement *fontFaceDecls =
      stylesHandle.FirstChildElement("office:document-styles")
          .FirstChildElement("office:font-face-decls")
          .ToElement();
  if (fontFaceDecls != nullptr) {
    StyleTranslator::css(*fontFaceDecls, context);
  }

  const tinyxml2::XMLElement *styles =
      stylesHandle.FirstChildElement("office:document-styles")
          .FirstChildElement("office:styles")
          .ToElement();
  if (styles != nullptr) {
    StyleTranslator::css(*styles, context);
  }

  const tinyxml2::XMLElement *automaticStyles =
      stylesHandle.FirstChildElement("office:document-styles")
          .FirstChildElement("office:automatic-styles")
          .ToElement();
  if (automaticStyles != nullptr) {
    StyleTranslator::css(*automaticStyles, context);
  }

  const tinyxml2::XMLElement *masterStyles =
      stylesHandle.FirstChildElement("office:document-styles")
          .FirstChildElement("office:master-styles")
          .ToElement();
  if (masterStyles != nullptr) {
    StyleTranslator::css(*masterStyles, context);
  }
}

void generateContentStyle_(tinyxml2::XMLHandle &in, Context &context) {
  const tinyxml2::XMLElement *fontFaceDecls =
      in.FirstChildElement("office:document-content")
          .FirstChildElement("office:font-face-decls")
          .ToElement();
  if (fontFaceDecls != nullptr) {
    StyleTranslator::css(*fontFaceDecls, context);
  }

  const tinyxml2::XMLElement *automaticStyles =
      in.FirstChildElement("office:document-content")
          .FirstChildElement("office:automatic-styles")
          .ToElement();
  if (automaticStyles != nullptr) {
    StyleTranslator::css(*automaticStyles, context);
  }
}

void generateScript_(std::ofstream &out, Context &) {
  out << common::Html::defaultScript();
}

void generateContent_(tinyxml2::XMLHandle &in, Context &context) {
  tinyxml2::XMLHandle bodyHandle =
      in.FirstChildElement("office:document-content")
          .FirstChildElement("office:body");
  const tinyxml2::XMLElement *body = bodyHandle.ToElement();

  tinyxml2::XMLElement *content = nullptr;
  std::string entryName;
  switch (context.meta->type) {
  case FileType::OPENDOCUMENT_TEXT:
  case FileType::OPENDOCUMENT_GRAPHICS:
    break;
  case FileType::OPENDOCUMENT_PRESENTATION:
    content = bodyHandle.FirstChildElement("office:presentation").ToElement();
    entryName = "draw:page";
    break;
  case FileType::OPENDOCUMENT_SPREADSHEET:
    content = bodyHandle.FirstChildElement("office:spreadsheet").ToElement();
    entryName = "table:table";
    break;
  default:
    throw std::invalid_argument("type");
  }

  context.entry = 0;

  if ((content != nullptr) &&
      ((context.config->entryOffset > 0) || (context.config->entryCount > 0))) {
    std::uint32_t i = 0;
    common::XmlUtil::visitElementChildren(
        *content, [&](const tinyxml2::XMLElement &c) {
          if (c.Name() != entryName)
            return;
          if ((i >= context.config->entryOffset) &&
              ((context.config->entryCount == 0) ||
               (i <
                context.config->entryOffset + context.config->entryCount))) {
            ContentTranslator::html(c, context);
          } else {
            ++context.entry; // TODO hacky
          }
          ++i;
        });
  } else {
    ContentTranslator::html(*body, context);
  }
}
} // namespace

class OpenDocument::Impl {
public:
  explicit Impl(const char *path) : Impl(access::Path(path)) {}

  explicit Impl(const std::string &path) : Impl(access::Path(path)) {}

  explicit Impl(const access::Path &path)
      : Impl(
            std::unique_ptr<access::ReadStorage>(new access::ZipReader(path))) {
  }

  explicit Impl(std::unique_ptr<access::ReadStorage> &&storage) {
    meta_ = Meta::parseFileMeta(*storage, false);
    manifest_ = Meta::parseManifest(*storage);

    storage_ = std::move(storage);
  }

  explicit Impl(std::unique_ptr<access::ReadStorage> &storage) {
    meta_ = Meta::parseFileMeta(*storage, false);
    manifest_ = Meta::parseManifest(*storage);

    storage_ = std::move(storage);
  }

  FileType type() const noexcept { return meta_.type; }

  bool encrypted() const noexcept { return meta_.encrypted; }

  const FileMeta &meta() const noexcept { return meta_; }

  const access::ReadStorage &storage() const noexcept { return *storage_; }

  bool decrypted() const noexcept { return decrypted_; }

  bool translatable() const noexcept { return true; }

  bool editable() const noexcept { return true; }

  bool savable(const bool encrypted) const noexcept {
    if (encrypted)
      return false;
    return !meta_.encrypted;
  }

  bool decrypt(const std::string &password) {
    // TODO throw if not encrypted
    // TODO throw if decrypted
    const bool success = Crypto::decrypt(storage_, manifest_, password);
    if (success)
      meta_ = Meta::parseFileMeta(*storage_, true);
    decrypted_ = success;
    return success;
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

    content_ = common::XmlUtil::parse(*storage_, "content.xml");
    tinyxml2::XMLHandle contentHandle(content_.get());

    out << common::Html::doctype();
    out << "<html><head>";
    out << common::Html::defaultHeaders();
    out << "<style>";
    generateStyle_(out, context_);
    generateContentStyle_(contentHandle, context_);
    out << "</style>";
    out << "</head>";

    out << "<body " << common::Html::bodyAttributes(config) << ">";
    generateContent_(contentHandle, context_);
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

  bool edit(const std::string &diff) {
    // TODO throw if not decrypted
    const auto json = nlohmann::json::parse(diff);

    if (json.contains("modifiedText")) {
      for (auto &&i : json["modifiedText"].items()) {
        const auto it = context_.textTranslation.find(std::stoi(i.key()));
        // TODO dirty const off-cast
        if (it == context_.textTranslation.end())
          continue;
        ((tinyxml2::XMLText *)it->second)
            ->SetValue(i.value().get<std::string>().c_str());
      }
    }

    return true;
  }

  bool save(const access::Path &path) const {
    // TODO throw if not decrypted
    // TODO this would decrypt/inflate and encrypt/deflate again
    access::ZipWriter writer(path);

    // `mimetype` has to be the first file and uncompressed
    if (storage_->isFile("mimetype")) {
      const auto in = storage_->read("mimetype");
      const auto out = writer.write("mimetype", 0);
      access::StreamUtil::pipe(*in, *out);
    }

    storage_->visit([&](const auto &p) {
      std::cout << p.string() << std::endl;
      if (p == "mimetype")
        return;
      if (storage_->isDirectory(p)) {
        writer.createDirectory(p);
        return;
      }
      const auto in = storage_->read(p);
      const auto out = writer.write(p);
      if (p == "content.xml") {
        tinyxml2::XMLPrinter printer(nullptr, true, 0);
        content_->Print(&printer);
        out->write(printer.CStr(), printer.CStrSize() - 1);
        return;
      }
      access::StreamUtil::pipe(*in, *out);
    });

    return true;
  }

  bool save(const access::Path &path, const std::string &password) const {
    // TODO throw if not decrypted
    return false;
  }

private:
  std::unique_ptr<access::ReadStorage> storage_;

  FileMeta meta_;
  Meta::Manifest manifest_;

  bool decrypted_{false};

  Context context_;
  std::unique_ptr<tinyxml2::XMLDocument> style_;
  std::unique_ptr<tinyxml2::XMLDocument> content_;
};

OpenDocument::OpenDocument(const char *path)
    : impl_(std::make_unique<Impl>(path)) {}

OpenDocument::OpenDocument(const std::string &path)
    : impl_(std::make_unique<Impl>(path)) {}

OpenDocument::OpenDocument(const access::Path &path)
    : impl_(std::make_unique<Impl>(path)) {}

OpenDocument::OpenDocument(std::unique_ptr<access::ReadStorage> &&storage)
    : impl_(std::make_unique<Impl>(storage)) {}

OpenDocument::OpenDocument(std::unique_ptr<access::ReadStorage> &storage)
    : impl_(std::make_unique<Impl>(storage)) {}

OpenDocument::OpenDocument(OpenDocument &&) noexcept = default;

OpenDocument &OpenDocument::operator=(OpenDocument &&) noexcept = default;

OpenDocument::~OpenDocument() = default;

const FileMeta &OpenDocument::meta() const noexcept { return impl_->meta(); }

const access::ReadStorage &OpenDocument::storage() const noexcept {
  return impl_->storage();
}

bool OpenDocument::decrypted() const noexcept { return impl_->decrypted(); }

bool OpenDocument::translatable() const noexcept {
  return impl_->translatable();
}

bool OpenDocument::editable() const noexcept { return impl_->editable(); }

bool OpenDocument::savable(const bool encrypted) const noexcept {
  return impl_->savable(encrypted);
}

bool OpenDocument::decrypt(const std::string &password) {
  return impl_->decrypt(password);
}

void OpenDocument::translate(const access::Path &path, const Config &config) {
  impl_->translate(path, config);
}

void OpenDocument::edit(const std::string &diff) { impl_->edit(diff); }

void OpenDocument::save(const access::Path &path) const { impl_->save(path); }

void OpenDocument::save(const access::Path &path,
                        const std::string &password) const {
  impl_->save(path, password);
}

} // namespace odf
} // namespace odr
