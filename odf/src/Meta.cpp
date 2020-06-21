#include <Meta.h>
#include <access/Storage.h>
#include <access/StorageUtil.h>
#include <common/MapUtil.h>
#include <common/TableCursor.h>
#include <common/XmlUtil.h>
#include <crypto/CryptoUtil.h>
#include <cstring>
#include <odr/Meta.h>
#include <pugixml.hpp>
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

void estimateTableDimensions(const pugi::xml_node &table, std::uint32_t &rows,
                             std::uint32_t &cols, const std::uint32_t limitRows,
                             const std::uint32_t limitCols) {
  rows = 0;
  cols = 0;

  common::TableCursor tl;

  auto range = table.select_nodes(".//self::table:table-row | .//self::table:table-cell");
  range.sort();
  for (auto &&e : range) {
    const auto &&n = e.node();
    if (std::strcmp(n.name(), "table:table-row") == 0) {
      const auto repeated =
          n.attribute("table:number-rows-repeated").as_uint(1);
      tl.addRow(repeated);
    } else if (std::strcmp(n.name(), "table:table-cell") == 0) {
      const auto repeated =
          n.attribute("table:number-columns-repeated").as_uint(1);
      const auto colspan =
          n.attribute("table:number-columns-spanned").as_uint(1);
      const auto rowspan = n.attribute("table:number-rows-spanned").as_uint(1);
      tl.addCell(colspan, rowspan, repeated);

      const auto newRows = tl.row();
      const auto newCols = std::max(cols, tl.col());
      if (n.first_child() && (((limitRows != 0) && (newRows < limitRows)) &&
                              ((limitCols != 0) && (newCols < limitCols)))) {
        rows = newRows;
        cols = newCols;
      }
    }
  }
}
} // namespace

FileMeta Meta::parseFileMeta(const access::ReadStorage &storage,
                             const bool decrypted) {
  FileMeta result;
  result.confident = true;

  if (!storage.isFile("content.xml"))
    throw NoOpenDocumentFileException();

  if (storage.isFile("mimetype")) {
    const auto mimeType = access::StorageUtil::read(storage, "mimetype");
    lookupFileType(mimeType, result.type);
  }

  if (storage.isFile("META-INF/manifest.xml")) {
    const auto manifest =
        common::XmlUtil::parse(storage, "META-INF/manifest.xml");
    for (auto &&e : manifest.select_nodes("//manifest:file-entry")) {
      const access::Path path =
          e.node().attribute("manifest:full-path").as_string();
      if (path.root() && e.node().attribute("manifest:media-type")) {
        const std::string mimeType =
            e.node().attribute("manifest:media-type").as_string();
        lookupFileType(mimeType, result.type);
      }
    }
    if (!manifest.select_nodes("//manifest:encryption-data").empty()) {
      result.encrypted = true;
    }
  }

  if (result.encrypted == decrypted) {
    if (storage.isFile("meta.xml")) {
      const auto metaXml = common::XmlUtil::parse(storage, "meta.xml");

      const pugi::xml_node statistics = metaXml.child("office:document-meta")
                                            .child("office:meta")
                                            .child("meta:document-statistic");
      if (statistics) {
        switch (result.type) {
        case FileType::OPENDOCUMENT_TEXT: {
          const auto pageCount = statistics.attribute("meta:page-count");
          if (!pageCount)
            break;
          result.entryCount = pageCount.as_uint();
        } break;
        case FileType::OPENDOCUMENT_PRESENTATION: {
          result.entryCount = 0;
        } break;
        case FileType::OPENDOCUMENT_SPREADSHEET: {
          const auto tableCount = statistics.attribute("meta:table-count");
          if (!tableCount)
            break;
          result.entryCount = tableCount.as_uint();
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
    const auto body =
        contentXml.child("office:document-content").child("office:body");
    if (!body)
      throw NoOpenDocumentFileException();

    switch (result.type) {
    case FileType::OPENDOCUMENT_GRAPHICS:
    case FileType::OPENDOCUMENT_PRESENTATION: {
      result.entryCount = 0;
      for (auto &&e : body.select_nodes("//draw:page")) {
        ++result.entryCount;
        FileMeta::Entry entry;
        entry.name = e.node().attribute("draw:name").as_string();
        result.entries.emplace_back(entry);
      }
    } break;
    case FileType::OPENDOCUMENT_SPREADSHEET: {
      result.entryCount = 0;
      for (auto &&e : body.select_nodes("//table:table")) {
        ++result.entryCount;
        FileMeta::Entry entry;
        entry.name = e.node().attribute("table:name").as_string();
        // TODO configuration
        estimateTableDimensions(e.node(), entry.rowCount, entry.columnCount,
                                10000, 500);
        result.entries.emplace_back(entry);
      }
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
  return parseManifest(manifest);
}

Meta::Manifest Meta::parseManifest(const pugi::xml_document &manifest) {
  Manifest result;

  for (auto &&e : manifest.child("manifest:manifest").children()) {
    const access::Path path = e.attribute("manifest:full-path").as_string();
    const pugi::xml_node crypto = e.child("manifest:encryption-data");
    if (!crypto)
      continue;
    result.encrypted = true;

    Manifest::Entry entry;
    entry.size = e.attribute("manifest:size").as_uint();

    { // checksum
      const std::string checksumType =
          crypto.attribute("manifest:checksum-type").as_string();
      lookupChecksumType(checksumType, entry.checksumType);
      entry.checksum = crypto.attribute("manifest:checksum").as_string();
    }

    { // encryption algorithm
      const pugi::xml_node algorithm = crypto.child("manifest:algorithm");
      const std::string algorithmName =
          algorithm.attribute("manifest:algorithm-name").as_string();
      lookupAlgorithmTypes(algorithmName, entry.algorithm);
      entry.initialisationVector =
          algorithm.attribute("manifest:initialisation-vector").as_string();
    }

    { // key derivation
      const pugi::xml_node key = crypto.child("manifest:key-derivation");
      const std::string keyDerivationName =
          key.attribute("manifest:key-derivation-name").as_string();
      lookupKeyDerivationTypes(keyDerivationName, entry.keyDerivation);
      entry.keySize = key.attribute("manifest:key-size").as_uint(16);
      entry.keyIterationCount =
          key.attribute("manifest:iteration-count").as_uint();
      entry.keySalt = key.attribute("manifest:salt").as_string();
    }

    { // start key generation
      const pugi::xml_node start =
          crypto.child("manifest:start-key-generation");
      if (start) {
        const std::string startKeyGenerationName =
            start.attribute("manifest:start-key-generation-name").as_string();
        lookupStartKeyTypes(startKeyGenerationName, entry.startKeyGeneration);
        entry.startKeySize = start.attribute("manifest:key-size").as_uint();
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
  }

  return result;
}

} // namespace odf
} // namespace odr
