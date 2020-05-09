#include <Meta.h>
#include <access/Storage.h>
#include <access/StorageUtil.h>
#include <common/MapUtil.h>
#include <common/TableCursor.h>
#include <common/XmlUtil.h>
#include <crypto/CryptoUtil.h>
#include <odr/Meta.h>
#include <tinyxml2.h>
#include <unordered_map>

namespace odr {
namespace odf {

namespace {
bool lookupFileType(const std::string &mimeType, FileType &fileType) {
  // https://www.openoffice.org/framework/documentation/mimetypes/mimetypes.html
  static const std::unordered_map<std::string, FileType> MIME_TYPES = {
      {"application/vnd.oasis.opendocument.text", FileType::OPENDOCUMENT_TEXT},
      {"application/vnd.oasis.opendocument.presentation",
       FileType::OPENDOCUMENT_PRESENTATION},
      {"application/vnd.oasis.opendocument.spreadsheet",
       FileType::OPENDOCUMENT_SPREADSHEET},
      {"application/vnd.oasis.opendocument.graphics",
       FileType::OPENDOCUMENT_GRAPHICS},
      // TODO any difference for template files?
      {"application/vnd.oasis.opendocument.text-template",
       FileType::OPENDOCUMENT_TEXT},
      {"application/vnd.oasis.opendocument.presentation-template",
       FileType::OPENDOCUMENT_PRESENTATION},
      {"application/vnd.oasis.opendocument.spreadsheet-template",
       FileType::OPENDOCUMENT_SPREADSHEET},
      {"application/vnd.oasis.opendocument.graphics-template",
       FileType::OPENDOCUMENT_GRAPHICS},
      // TODO these staroffice types might deserve their own type
      {"application/vnd.sun.xml.writer", FileType::OPENDOCUMENT_TEXT},
      {"application/vnd.sun.xml.impress", FileType::OPENDOCUMENT_PRESENTATION},
      {"application/vnd.sun.xml.calc", FileType::OPENDOCUMENT_SPREADSHEET},
      {"application/vnd.sun.xml.draw", FileType::OPENDOCUMENT_GRAPHICS},
      // TODO any difference for template files?
      {"application/vnd.sun.xml.writer.template", FileType::OPENDOCUMENT_TEXT},
      {"application/vnd.sun.xml.impress.template",
       FileType::OPENDOCUMENT_PRESENTATION},
      {"application/vnd.sun.xml.calc.template",
       FileType::OPENDOCUMENT_SPREADSHEET},
      {"application/vnd.sun.xml.draw.template",
       FileType::OPENDOCUMENT_GRAPHICS},
  };
  return common::MapUtil::lookupMapDefault(MIME_TYPES, mimeType, fileType,
                                           FileType::UNKNOWN);
}

bool lookupChecksumType(const std::string &checksum,
                        Meta::ChecksumType &checksumType) {
  static const std::unordered_map<std::string, Meta::ChecksumType>
      CHECKSUM_TYPES = {
          {"SHA1", Meta::ChecksumType::SHA1},
          {"SHA1/1K", Meta::ChecksumType::SHA1_1K},
          {"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0#sha256-1k",
           Meta::ChecksumType::SHA256_1K},
      };
  return common::MapUtil::lookupMapDefault(
      CHECKSUM_TYPES, checksum, checksumType, Meta::ChecksumType::UNKNOWN);
}

bool lookupAlgorithmTypes(const std::string &algorithm,
                          Meta::AlgorithmType &algorithmType) {
  static const std::unordered_map<std::string, Meta::AlgorithmType>
      ALGORITHM_TYPES = {
          {"http://www.w3.org/2001/04/xmlenc#aes256-cbc",
           Meta::AlgorithmType::AES256_CBC},
          {"", Meta::AlgorithmType::TRIPLE_DES_CBC},
          {"Blowfish CFB", Meta::AlgorithmType::BLOWFISH_CFB},
      };
  return common::MapUtil::lookupMapDefault(
      ALGORITHM_TYPES, algorithm, algorithmType, Meta::AlgorithmType::UNKNOWN);
}

bool lookupKeyDerivationTypes(const std::string &keyDerivation,
                              Meta::KeyDerivationType &keyDerivationType) {
  static const std::unordered_map<std::string, Meta::KeyDerivationType>
      KEY_DERIVATION_TYPES = {
          {"PBKDF2", Meta::KeyDerivationType::PBKDF2},
      };
  return common::MapUtil::lookupMapDefault(KEY_DERIVATION_TYPES, keyDerivation,
                                           keyDerivationType,
                                           Meta::KeyDerivationType::UNKNOWN);
}

bool lookupStartKeyTypes(const std::string &checksum,
                         Meta::ChecksumType &checksumType) {
  static const std::unordered_map<std::string, Meta::ChecksumType>
      STARTKEY_TYPES = {
          {"SHA1", Meta::ChecksumType::SHA1},
          {"http://www.w3.org/2000/09/xmldsig#sha256",
           Meta::ChecksumType::SHA256},
      };
  return common::MapUtil::lookupMapDefault(
      STARTKEY_TYPES, checksum, checksumType, Meta::ChecksumType::UNKNOWN);
}

void estimateTableDimensions(const tinyxml2::XMLElement &table,
                             std::uint32_t &rows, std::uint32_t &cols,
                             const std::uint32_t limitRows,
                             const std::uint32_t limitCols) {
  rows = 0;
  cols = 0;

  common::TableCursor tl;

  // TODO we dont need to recurse so deep
  common::XmlUtil::recursiveVisitElements(&table, [&](const auto &e) {
    if (e.Name() == std::string("table:table-row")) {
      const auto repeated =
          e.Unsigned64Attribute("table:number-rows-repeated", 1);
      tl.addRow(repeated);
    } else if (e.Name() == std::string("table:table-cell")) {
      const auto repeated =
          e.Unsigned64Attribute("table:number-columns-repeated", 1);
      const auto colspan =
          e.Unsigned64Attribute("table:number-columns-spanned", 1);
      const auto rowspan =
          e.Unsigned64Attribute("table:number-rows-spanned", 1);
      tl.addCell(colspan, rowspan, repeated);

      const auto newRows = tl.row();
      const auto newCols = std::max(cols, tl.col());
      if ((e.FirstChild() != nullptr) &&
          (((limitRows != 0) && (newRows < limitRows)) &&
           ((limitCols != 0) && (newCols < limitCols)))) {
        rows = newRows;
        cols = newCols;
      }
    }
  });
}
} // namespace

FileMeta Meta::parseFileMeta(const access::ReadStorage &storage,
                             const bool decrypted) {
  FileMeta result{};

  if (!storage.isFile("content.xml"))
    throw NoOpenDocumentFileException();

  if (storage.isFile("mimetype")) {
    const auto mimeType = access::StorageUtil::read(storage, "mimetype");
    lookupFileType(mimeType, result.type);
  }

  if (storage.isFile("META-INF/manifest.xml")) {
    const auto manifest =
        common::XmlUtil::parse(storage, "META-INF/manifest.xml");
    common::XmlUtil::recursiveVisitElementsWithName(
        manifest->RootElement(), "manifest:file-entry",
        [&](const tinyxml2::XMLElement &e) {
          const access::Path path =
              e.FindAttribute("manifest:full-path")->Value();
          if (path == "/" &&
              e.FindAttribute("manifest:media-type") != nullptr) {
            const std::string mimeType =
                e.FindAttribute("manifest:media-type")->Value();
            lookupFileType(mimeType, result.type);
          }
        });
    common::XmlUtil::recursiveVisitElementsWithName(
        manifest->RootElement(), "manifest:encryption-data",
        [&](const tinyxml2::XMLElement &) { result.encrypted = true; });
  }

  if (result.encrypted == decrypted) {
    if (storage.isFile("meta.xml")) {
      const auto metaXml = common::XmlUtil::parse(storage, "meta.xml");

      tinyxml2::XMLElement *statisticsElement =
          tinyxml2::XMLHandle(metaXml.get())
              .FirstChildElement("office:document-meta")
              .FirstChildElement("office:meta")
              .FirstChildElement("meta:document-statistic")
              .ToElement();
      if (statisticsElement != nullptr) {
        switch (result.type) {
        case FileType::OPENDOCUMENT_TEXT: {
          const tinyxml2::XMLAttribute *pageCount =
              statisticsElement->FindAttribute("meta:page-count");
          if (pageCount == nullptr) {
            break;
          }
          result.entryCount = pageCount->UnsignedValue();
        } break;
        case FileType::OPENDOCUMENT_PRESENTATION: {
          result.entryCount = 0;
        } break;
        case FileType::OPENDOCUMENT_SPREADSHEET: {
          const tinyxml2::XMLAttribute *tableCount =
              statisticsElement->FindAttribute("meta:table-count");
          if (tableCount == nullptr) {
            break;
          }
          result.entryCount = tableCount->UnsignedValue();
        } break;
        case FileType::OPENDOCUMENT_GRAPHICS: {
        } break;
        default:
          break;
        }
      }
    }

    // TODO dont load content twice (happens in case of translation)
    const auto contentXml = common::XmlUtil::parse(storage, "content.xml");
    tinyxml2::XMLHandle bodyHandle =
        tinyxml2::XMLHandle(contentXml.get())
            .FirstChildElement("office:document-content")
            .FirstChildElement("office:body");
    if (bodyHandle.ToElement() == nullptr)
      throw NoOpenDocumentFileException();

    switch (result.type) {
    case FileType::OPENDOCUMENT_GRAPHICS:
    case FileType::OPENDOCUMENT_PRESENTATION: {
      result.entryCount = 0;
      common::XmlUtil::recursiveVisitElementsWithName(
          bodyHandle.ToElement(), "draw:page",
          [&](const tinyxml2::XMLElement &e) {
            ++result.entryCount;
            FileMeta::Entry entry;
            entry.name = e.FindAttribute("draw:name")->Value();
            result.entries.emplace_back(entry);
          });
    } break;
    case FileType::OPENDOCUMENT_SPREADSHEET: {
      result.entryCount = 0;
      common::XmlUtil::recursiveVisitElementsWithName(
          bodyHandle.ToElement(), "table:table",
          [&](const tinyxml2::XMLElement &e) {
            ++result.entryCount;
            FileMeta::Entry entry;
            entry.name = e.FindAttribute("table:name")->Value();
            // TODO configuration
            estimateTableDimensions(e, entry.rowCount, entry.columnCount, 10000,
                                    500);
            result.entries.emplace_back(entry);
          });
    } break;
    default:
      break;
    }
  }

  return result;
}

Meta::Manifest Meta::parseManifest(const access::ReadStorage &storage) {
  if (!storage.isFile("META-INF/manifest.xml"))
    throw NoOpenDocumentFileException();
  const auto manifest =
      common::XmlUtil::parse(storage, "META-INF/manifest.xml");
  return parseManifest(*manifest);
}

Meta::Manifest Meta::parseManifest(const tinyxml2::XMLDocument &manifest) {
  Manifest result{};

  common::XmlUtil::recursiveVisitElementsWithName(
      manifest.RootElement(), "manifest:file-entry",
      [&](const tinyxml2::XMLElement &e) {
        const access::Path path =
            e.FindAttribute("manifest:full-path")->Value();
        const tinyxml2::XMLElement *crypto =
            e.FirstChildElement("manifest:encryption-data");
        if (crypto == nullptr)
          return;
        result.encrypted = true;

        Manifest::Entry entry{};
        entry.size = e.FindAttribute("manifest:size")->UnsignedValue();

        { // checksum
          const std::string checksumType =
              crypto->FindAttribute("manifest:checksum-type")->Value();
          lookupChecksumType(checksumType, entry.checksumType);
          entry.checksum = crypto->FindAttribute("manifest:checksum")->Value();
        }

        { // encryption algorithm
          const tinyxml2::XMLElement *algorithm =
              crypto->FirstChildElement("manifest:algorithm");
          const std::string algorithmName =
              algorithm->FindAttribute("manifest:algorithm-name")->Value();
          lookupAlgorithmTypes(algorithmName, entry.algorithm);
          entry.initialisationVector =
              algorithm->FindAttribute("manifest:initialisation-vector")
                  ->Value();
        }

        { // key derivation
          const tinyxml2::XMLElement *key =
              crypto->FirstChildElement("manifest:key-derivation");
          const std::string keyDerivationName =
              key->FindAttribute("manifest:key-derivation-name")->Value();
          lookupKeyDerivationTypes(keyDerivationName, entry.keyDerivation);
          entry.keySize = key->Unsigned64Attribute("manifest:key-size", 16);
          entry.keyIterationCount =
              key->FindAttribute("manifest:iteration-count")->UnsignedValue();
          entry.keySalt = key->FindAttribute("manifest:salt")->Value();
        }

        { // start key generation
          const tinyxml2::XMLElement *start =
              crypto->FirstChildElement("manifest:start-key-generation");
          if (start != nullptr) {
            const std::string startKeyGenerationName =
                start->FindAttribute("manifest:start-key-generation-name")
                    ->Value();
            lookupStartKeyTypes(startKeyGenerationName,
                                entry.startKeyGeneration);
            entry.startKeySize =
                start->FindAttribute("manifest:key-size")->UnsignedValue();
          } else {
            entry.startKeyGeneration = ChecksumType::SHA1;
            entry.startKeySize = 20;
          }
        }

        entry.checksum = crypto::Util::base64Decode(entry.checksum);
        entry.initialisationVector =
            crypto::Util::base64Decode(entry.initialisationVector);
        entry.keySalt = crypto::Util::base64Decode(entry.keySalt);

        const auto it = result.entries.emplace(path, entry).first;
        if ((result.smallestFilePath == nullptr) ||
            (entry.size < result.smallestFileSize)) {
          result.smallestFileSize = entry.size;
          result.smallestFilePath = &it->first;
          result.smallestFileEntry = &it->second;
        }
      });

  return result;
}

} // namespace odf
} // namespace odr
