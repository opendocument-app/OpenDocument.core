#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/common/archive.h>
#include <internal/common/html.h>
#include <internal/common/path.h>
#include <internal/ooxml/ooxml_crypto.h>
#include <internal/ooxml/ooxml_document_translator.h>
#include <internal/ooxml/ooxml_meta.h>
#include <internal/ooxml/ooxml_presentation_translator.h>
#include <internal/ooxml/ooxml_translator.h>
#include <internal/ooxml/ooxml_translator_context.h>
#include <internal/ooxml/ooxml_workbook_translator.h>
#include <internal/util/stream_util.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file_meta.h>
#include <odr/html_config.h>
#include <pugixml.hpp>

namespace odr::internal::ooxml {

namespace {
void generate_style(std::ofstream &out, Context &context) {
  // default css
  out << common::Html::default_style();

  switch (context.meta->type) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT: {
    const auto styles =
        util::xml::parse(*context.filesystem, "word/styles.xml");
    document_translator::css(styles.document_element(), context);
  } break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    // TODO that should go to `PresentationTranslator::css`

    // TODO duplication in generate_content
    const auto ppt =
        util::xml::parse(*context.filesystem, "ppt/presentation.xml");
    const auto size_ele = ppt.select_node("//p:sldSz").node();
    if (!size_ele) {
      break;
    }
    const float width_in = size_ele.attribute("cx").as_float() / 914400.0f;
    const float height_in = size_ele.attribute("cy").as_float() / 914400.0f;

    out << ".slide {";
    out << "width:" << width_in << "in;";
    out << "height:" << height_in << "in;";
    out << "}";
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto styles = util::xml::parse(*context.filesystem, "xl/styles.xml");
    workbook_translator::css(styles.document_element(), context);
  } break;
  default:
    throw std::invalid_argument("file.getMeta().type");
  }
}

void generate_script(std::ofstream &out, Context &) {
  out << common::Html::default_script();
}

void generate_content(Context &context) {
  context.entry = 0;

  switch (context.meta->type) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT: {
    const auto content =
        util::xml::parse(*context.filesystem, "word/document.xml");
    context.relations =
        parse_relationships(*context.filesystem, "word/document.xml");

    const auto body = content.child("w:document").child("w:body");
    document_translator::html(body, context);
  } break;
  case FileType::OFFICE_OPEN_XML_PRESENTATION: {
    const auto ppt =
        util::xml::parse(*context.filesystem, "ppt/presentation.xml");
    const auto ppt_relations =
        parse_relationships(*context.filesystem, "ppt/presentation.xml");

    for (auto &&e : ppt.select_nodes("//p:sldId")) {
      const std::string rId = e.node().attribute("r:id").as_string();

      const auto path = common::Path("ppt").join(ppt_relations.at(rId));
      const auto content = util::xml::parse(*context.filesystem, path);
      context.relations = parse_relationships(*context.filesystem, path);

      if ((context.config->entry_offset > 0) ||
          (context.config->entry_count > 0)) {
        if ((context.entry >= context.config->entry_offset) &&
            (context.entry <
             context.config->entry_offset + context.config->entry_count)) {
          presentation_translator::html(content, context);
        }
      } else {
        presentation_translator::html(content, context);
      }

      ++context.entry;
    }
  } break;
  case FileType::OFFICE_OPEN_XML_WORKBOOK: {
    const auto xls = util::xml::parse(*context.filesystem, "xl/workbook.xml");
    const auto xls_relations =
        parse_relationships(*context.filesystem, "xl/workbook.xml");

    // TODO this breaks back translation
    pugi::xml_document shared_strings;
    if (context.filesystem->is_file("xl/sharedStrings.xml")) {
      shared_strings =
          util::xml::parse(*context.filesystem, "xl/sharedStrings.xml");
      for (auto &&e : shared_strings.select_nodes("//si")) {
        context.shared_strings.push_back(e.node());
      }
    }

    for (auto &&e : xls.select_nodes("//sheet")) {
      const std::string rId = e.node().attribute("r:id").as_string();

      const auto path = common::Path("xl").join(xls_relations.at(rId));
      const auto content = util::xml::parse(*context.filesystem, path);
      context.relations = parse_relationships(*context.filesystem, path);

      if ((context.config->entry_offset > 0) ||
          (context.config->entry_count > 0)) {
        if ((context.entry >= context.config->entry_offset) &&
            (context.entry <
             context.config->entry_offset + context.config->entry_count)) {
          workbook_translator::html(content, context);
        }
      } else {
        workbook_translator::html(content, context);
      }

      ++context.entry;
    }
  } break;
  default:
    throw std::invalid_argument("file.getMeta().type");
  }
}
} // namespace

OfficeOpenXmlTranslator::OfficeOpenXmlTranslator(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_meta = parse_file_meta(*m_filesystem);
}

OfficeOpenXmlTranslator::~OfficeOpenXmlTranslator() = default;

const FileMeta &OfficeOpenXmlTranslator::meta() const noexcept {
  return m_meta;
}

bool OfficeOpenXmlTranslator::decrypted() const noexcept { return m_decrypted; }

bool OfficeOpenXmlTranslator::translatable() const noexcept { return true; }

bool OfficeOpenXmlTranslator::editable() const noexcept { return false; }

bool OfficeOpenXmlTranslator::savable(const bool) const noexcept {
  return false;
}

bool OfficeOpenXmlTranslator::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  // TODO throw if decrypted
  const std::string encryption_info =
      util::stream::read(*m_filesystem->open("/EncryptionInfo")->read());
  // TODO cache Crypto::Util
  Crypto::Util util(encryption_info);
  const std::string key = util.derive_key(password);
  if (!util.verify(key)) {
    return false;
  }
  const std::string encrypted_package =
      util::stream::read(*m_filesystem->open("/EncryptedPackage")->read());
  const std::string decrypted_package = util.decrypt(encrypted_package, key);
  common::ArchiveFile<zip::ReadonlyZipArchive> zip(
      std::make_shared<common::MemoryFile>(decrypted_package));
  m_filesystem = zip.archive()->filesystem();
  m_meta = parse_file_meta(*m_filesystem);
  m_decrypted = true;
  return true;
}

void OfficeOpenXmlTranslator::translate(const common::Path &path,
                                        const HtmlConfig &config) {
  // TODO throw if not decrypted
  std::ofstream out(path.path());
  if (!out.is_open()) {
    throw FileNotCreated();
  }

  m_context = {};
  m_context.config = &config;
  m_context.meta = &m_meta;
  m_context.filesystem = m_filesystem.get();
  m_context.output = &out;

  out << common::Html::doctype();
  out << "<html><head>";
  out << common::Html::default_headers();
  out << "<style>";
  generate_style(out, m_context);
  out << "</style>";
  out << "</head>";

  out << "<body " << common::Html::body_attributes(config) << ">";
  generate_content(m_context);
  out << "</body>";

  out << "<script>";
  generate_script(out, m_context);
  out << "</script>";
  out << "</html>";

  m_context.config = nullptr;
  m_context.output = nullptr;
  out.close();
}

void OfficeOpenXmlTranslator::edit(const std::string &) {
  throw UnsupportedOperation();
}

void OfficeOpenXmlTranslator::save(const common::Path &) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlTranslator::save(const common::Path &,
                                   const std::string &) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
