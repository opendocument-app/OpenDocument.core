#include <odr/internal/ooxml/ooxml_blank.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <array>
#include <memory>
#include <span>
#include <sstream>
#include <string_view>

namespace odr::internal::ooxml {

namespace {

struct Part final {
  std::string_view path;
  std::string_view content;
};

constexpr std::array document_parts{
    Part{
        "[Content_Types].xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
        R"(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)"
        R"(<Default Extension="xml" ContentType="application/xml"/>)"
        R"(<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>)"
        R"(<Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>)"
        R"(<Override PartName="/word/settings.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml"/>)"
        R"(<Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>)"
        R"(</Types>)",
    },
    Part{
        "_rels/.rels",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>)"
        R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/>)"
        R"(</Relationships>)",
    },
    Part{
        "docProps/app.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties">)"
        R"(<Application>odr</Application>)"
        R"(</Properties>)",
    },
    Part{
        "word/_rels/document.xml.rels",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>)"
        R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings" Target="settings.xml"/>)"
        R"(</Relationships>)",
    },
    // A4 in twips, with the margins of 1 inch that Word sets
    Part{
        "word/document.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
        R"(<w:body><w:p/><w:sectPr>)"
        R"(<w:pgSz w:w="11906" w:h="16838"/>)"
        R"(<w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440" w:header="708" w:footer="708" w:gutter="0"/>)"
        R"(</w:sectPr></w:body></w:document>)",
    },
    Part{
        "word/styles.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">)"
        R"(<w:docDefaults>)"
        R"(<w:rPrDefault><w:rPr><w:rFonts w:ascii="Calibri" w:eastAsia="Calibri" w:hAnsi="Calibri" w:cs="Calibri"/><w:sz w:val="22"/><w:szCs w:val="22"/></w:rPr></w:rPrDefault>)"
        R"(<w:pPrDefault><w:pPr><w:spacing w:after="160" w:line="259" w:lineRule="auto"/></w:pPr></w:pPrDefault>)"
        R"(</w:docDefaults>)"
        R"(<w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:qFormat/></w:style>)"
        R"(<w:style w:type="character" w:default="1" w:styleId="DefaultParagraphFont"><w:name w:val="Default Paragraph Font"/><w:uiPriority w:val="1"/><w:semiHidden/><w:unhideWhenUsed/></w:style>)"
        R"(<w:style w:type="table" w:default="1" w:styleId="TableNormal"><w:name w:val="Normal Table"/><w:uiPriority w:val="99"/><w:semiHidden/><w:unhideWhenUsed/>)"
        R"(<w:tblPr><w:tblInd w:w="0" w:type="dxa"/><w:tblCellMar><w:top w:w="0" w:type="dxa"/><w:left w:w="108" w:type="dxa"/><w:bottom w:w="0" w:type="dxa"/><w:right w:w="108" w:type="dxa"/></w:tblCellMar></w:tblPr></w:style>)"
        R"(</w:styles>)",
    },
    // without `compatibilityMode` 15 Word opens the file in compatibility mode
    Part{
        "word/settings.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<w:settings xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">)"
        R"(<w:defaultTabStop w:val="720"/>)"
        R"(<w:characterSpacingControl w:val="doNotCompress"/>)"
        R"(<w:compat><w:compatSetting w:name="compatibilityMode" w:uri="http://schemas.microsoft.com/office/word" w:val="15"/></w:compat>)"
        R"(</w:settings>)",
    },
};

std::span<const Part> find_parts(const FileType type) {
  switch (type) {
  case FileType::office_open_xml_document:
    return document_parts;
  default:
    throw UnsupportedFileType(type);
  }
}

} // namespace

std::string blank_package(const FileType type) {
  zip::ZipArchive archive;
  for (const Part &part : find_parts(type)) {
    archive.insert_file(
        std::end(archive), RelPath(std::string(part.path)),
        std::make_shared<MemoryFile>(std::string(part.content)));
  }

  std::stringstream out;
  archive.save(out);
  return std::move(out).str();
}

} // namespace odr::internal::ooxml
