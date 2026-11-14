#include <odr/internal/ooxml/ooxml_blank.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/blank_slide_layouts.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

namespace odr::internal::ooxml {

namespace {

struct Part final {
  std::string_view path;
  std::string_view content;
};

constexpr Part app_part{
    "docProps/app.xml",
    R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
    R"(<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties">)"
    R"(<Application>odr</Application>)"
    R"(</Properties>)",
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
    app_part,
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
        R"(<w:compat><w:compatSetting w:name="compatibilityMode" w:uri="http://schemas.microsoft.com/office/word" w:val="15"/></w:compat>)"
        R"(</w:settings>)",
    },
};

constexpr std::array workbook_parts{
    Part{
        "[Content_Types].xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
        R"(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)"
        R"(<Default Extension="xml" ContentType="application/xml"/>)"
        R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
        R"(<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
        R"(<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>)"
        R"(<Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>)"
        R"(</Types>)",
    },
    Part{
        "_rels/.rels",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
        R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/>)"
        R"(</Relationships>)",
    },
    app_part,
    Part{
        "xl/_rels/workbook.xml.rels",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
        R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>)"
        R"(</Relationships>)",
    },
    Part{
        "xl/workbook.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
        R"(<bookViews><workbookView/></bookViews>)"
        R"(<sheets><sheet name="Sheet1" sheetId="1" r:id="rId1"/></sheets>)"
        R"(</workbook>)",
    },
    // the page margins that Excel sets, in inches; `paperSize` 9 is A4
    Part{
        "xl/worksheets/sheet1.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
        R"(<dimension ref="A1"/>)"
        R"(<sheetViews><sheetView tabSelected="1" workbookViewId="0"/></sheetViews>)"
        R"(<sheetFormatPr defaultRowHeight="15"/>)"
        R"(<sheetData/>)"
        R"(<pageMargins left="0.7" right="0.7" top="0.75" bottom="0.75" header="0.3" footer="0.3"/>)"
        R"(<pageSetup paperSize="9" orientation="portrait"/>)"
        R"(</worksheet>)",
    },
    // Excel reserves the first two fills, `none` and `gray125`
    Part{
        "xl/styles.xml",
        R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
        R"(<fonts count="1"><font><sz val="11"/><name val="Calibri"/><family val="2"/></font></fonts>)"
        R"(<fills count="2"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill></fills>)"
        R"(<borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders>)"
        R"(<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>)"
        R"(<cellXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/></cellXfs>)"
        R"(<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>)"
        R"(</styleSheet>)",
    },
};

constexpr std::string_view presentation_namespaces =
    R"( xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main")"
    R"( xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships")"
    R"( xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main")";

constexpr std::string_view xml_declaration =
    R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)";

constexpr std::string_view relationship_type_prefix =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/";

/// The Office 2013-2022 theme, cut to what [ECMA-376] 20.1.6.9 requires: a
/// style matrix needs three entries in each list.
constexpr std::string_view theme =
    R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
    R"(<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="Office Theme"><a:themeElements>)"
    R"(<a:clrScheme name="Office">)"
    R"(<a:dk1><a:sysClr val="windowText" lastClr="000000"/></a:dk1><a:lt1><a:sysClr val="window" lastClr="FFFFFF"/></a:lt1>)"
    R"(<a:dk2><a:srgbClr val="44546A"/></a:dk2><a:lt2><a:srgbClr val="E7E6E6"/></a:lt2>)"
    R"(<a:accent1><a:srgbClr val="4472C4"/></a:accent1><a:accent2><a:srgbClr val="ED7D31"/></a:accent2>)"
    R"(<a:accent3><a:srgbClr val="A5A5A5"/></a:accent3><a:accent4><a:srgbClr val="FFC000"/></a:accent4>)"
    R"(<a:accent5><a:srgbClr val="5B9BD5"/></a:accent5><a:accent6><a:srgbClr val="70AD47"/></a:accent6>)"
    R"(<a:hlink><a:srgbClr val="0563C1"/></a:hlink><a:folHlink><a:srgbClr val="954F72"/></a:folHlink>)"
    R"(</a:clrScheme>)"
    R"(<a:fontScheme name="Office">)"
    R"(<a:majorFont><a:latin typeface="Calibri Light"/><a:ea typeface=""/><a:cs typeface=""/></a:majorFont>)"
    R"(<a:minorFont><a:latin typeface="Calibri"/><a:ea typeface=""/><a:cs typeface=""/></a:minorFont>)"
    R"(</a:fontScheme>)"
    R"(<a:fmtScheme name="Office">)"
    R"(<a:fillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:fillStyleLst>)"
    R"(<a:lnStyleLst>)"
    R"(<a:ln w="6350"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>)"
    R"(<a:ln w="12700"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>)"
    R"(<a:ln w="19050"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>)"
    R"(</a:lnStyleLst>)"
    R"(<a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle><a:effectStyle><a:effectLst/></a:effectStyle><a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst>)"
    R"(<a:bgFillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:bgFillStyleLst>)"
    R"(</a:fmtScheme>)"
    R"(</a:themeElements></a:theme>)";

/// [ECMA-376] 19.3.1.47 `p:txStyles`: the text a placeholder starts with.
constexpr std::string_view text_styles =
    R"(<p:txStyles>)"
    R"(<p:titleStyle><a:lvl1pPr algn="l"><a:lnSpc><a:spcPct val="90000"/></a:lnSpc><a:spcBef><a:spcPct val="0"/></a:spcBef><a:buNone/><a:defRPr sz="4400" kern="1200"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mj-lt"/><a:ea typeface="+mj-ea"/><a:cs typeface="+mj-cs"/></a:defRPr></a:lvl1pPr></p:titleStyle>)"
    R"(<p:bodyStyle><a:lvl1pPr marL="228600" indent="-228600" algn="l"><a:lnSpc><a:spcPct val="90000"/></a:lnSpc><a:spcBef><a:spcPts val="1000"/></a:spcBef><a:buFont typeface="Arial"/><a:buChar char="&#8226;"/><a:defRPr sz="2800" kern="1200"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/><a:ea typeface="+mn-ea"/><a:cs typeface="+mn-cs"/></a:defRPr></a:lvl1pPr></p:bodyStyle>)"
    R"(<p:otherStyle><a:lvl1pPr marL="0" algn="l"><a:defRPr sz="1800" kern="1200"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/><a:ea typeface="+mn-ea"/><a:cs typeface="+mn-cs"/></a:defRPr></a:lvl1pPr></p:otherStyle>)"
    R"(</p:txStyles>)";

constexpr std::string_view shape_tree_head =
    R"(<p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>)";

/// An EMU length from hundredths of a centimetre ([ECMA-376] 20.1.2.1).
std::int64_t emus(const std::int32_t hundredths) {
  return static_cast<std::int64_t>(hundredths) * 3600;
}

/// The `p:ph` of @p placeholder, the @p ordinal-th of its layout
/// ([ECMA-376] 19.3.1.36).
std::string placeholder_element(const BlankPlaceholder &placeholder,
                                const std::size_t ordinal) {
  switch (placeholder.kind) {
  case BlankPlaceholderKind::title:
    return R"(<p:ph type="title"/>)";
  case BlankPlaceholderKind::centered_title:
    return R"(<p:ph type="ctrTitle"/>)";
  case BlankPlaceholderKind::subtitle:
    return fmt::format(R"(<p:ph type="subTitle" idx="{}"/>)", ordinal);
  case BlankPlaceholderKind::content:
    return fmt::format(R"(<p:ph idx="{}"/>)", ordinal);
  case BlankPlaceholderKind::text:
    return fmt::format(R"(<p:ph type="body" idx="{}"/>)", ordinal);
  }
  throw std::invalid_argument("unknown placeholder kind");
}

std::string_view placeholder_name(const BlankPlaceholderKind kind) {
  switch (kind) {
  case BlankPlaceholderKind::title:
  case BlankPlaceholderKind::centered_title:
    return "Title";
  case BlankPlaceholderKind::subtitle:
    return "Subtitle";
  case BlankPlaceholderKind::content:
    return "Content Placeholder";
  case BlankPlaceholderKind::text:
    return "Text Placeholder";
  }
  throw std::invalid_argument("unknown placeholder kind");
}

/// The part a shape tree belongs to.
enum class Owner { master, layout, slide };

/// The `a:bodyPr` and `a:lstStyle` of a placeholder: a layout states how its
/// placeholders differ from the master's title and body, as PowerPoint's own
/// layouts do.
std::string_view text_body_head(const BlankPlaceholderKind kind,
                                const Owner owner) {
  if (owner == Owner::master) {
    return kind == BlankPlaceholderKind::title
               ? R"(<a:bodyPr anchor="ctr"/><a:lstStyle/>)"
               : R"(<a:bodyPr/><a:lstStyle/>)";
  }
  if (owner == Owner::slide) {
    return R"(<a:bodyPr/><a:lstStyle/>)";
  }
  switch (kind) {
  case BlankPlaceholderKind::centered_title:
    return R"(<a:bodyPr anchor="b"/><a:lstStyle><a:lvl1pPr algn="ctr"><a:defRPr sz="4400"/></a:lvl1pPr></a:lstStyle>)";
  case BlankPlaceholderKind::subtitle:
    return R"(<a:bodyPr/><a:lstStyle><a:lvl1pPr marL="0" indent="0" algn="ctr"><a:buNone/><a:defRPr sz="2400"/></a:lvl1pPr></a:lstStyle>)";
  case BlankPlaceholderKind::text:
    return R"(<a:bodyPr anchor="t"/><a:lstStyle><a:lvl1pPr marL="0" indent="0"><a:buNone/><a:defRPr sz="2400"/></a:lvl1pPr></a:lstStyle>)";
  case BlankPlaceholderKind::title:
  case BlankPlaceholderKind::content:
    return R"(<a:bodyPr/><a:lstStyle/>)";
  }
  throw std::invalid_argument("unknown placeholder kind");
}

/// The placeholders of a shape tree, each with an explicit position and one
/// empty paragraph.
std::string placeholder_shapes(const std::span<const BlankPlaceholder> shapes,
                               const Owner owner) {
  const bool master = owner == Owner::master;
  std::string result;
  for (std::size_t i = 0; i < shapes.size(); ++i) {
    const BlankPlaceholder &placeholder = shapes[i];
    const std::string ph =
        master && placeholder.kind == BlankPlaceholderKind::content
            ? fmt::format(R"(<p:ph type="body" idx="{}"/>)", i)
            : placeholder_element(placeholder, i);
    result += fmt::format(
        R"(<p:sp><p:nvSpPr><p:cNvPr id="{}" name="{} {}"/><p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr>{}</p:nvPr></p:nvSpPr>)"
        R"(<p:spPr><a:xfrm><a:off x="{}" y="{}"/><a:ext cx="{}" cy="{}"/></a:xfrm>{}</p:spPr>)"
        R"(<p:txBody>{}<a:p><a:endParaRPr lang="en-US"/></a:p></p:txBody></p:sp>)",
        i + 2, placeholder_name(placeholder.kind), i + 1, ph,
        emus(placeholder.x), emus(placeholder.y), emus(placeholder.width),
        emus(placeholder.height),
        master ? R"(<a:prstGeom prst="rect"><a:avLst/></a:prstGeom>)" : "",
        text_body_head(placeholder.kind, owner));
  }
  return result;
}

std::string relationships(
    const std::span<const std::pair<std::string_view, std::string>> targets) {
  std::string result(xml_declaration);
  result +=
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
  for (std::size_t i = 0; i < targets.size(); ++i) {
    result += fmt::format(
        R"(<Relationship Id="rId{}" Type="{}{}" Target="{}"/>)", i + 1,
        relationship_type_prefix, targets[i].first, targets[i].second);
  }
  result += "</Relationships>";
  return result;
}

std::vector<std::pair<std::string, std::string>> presentation_parts() {
  using Target = std::pair<std::string_view, std::string>;
  std::vector<std::pair<std::string, std::string>> result;

  std::string content_types(xml_declaration);
  content_types +=
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)"
      R"(<Default Extension="xml" ContentType="application/xml"/>)"
      R"(<Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>)"
      R"(<Override PartName="/ppt/presProps.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presProps+xml"/>)"
      R"(<Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>)"
      R"(<Override PartName="/ppt/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>)"
      R"(<Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>)"
      R"(<Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>)";
  for (std::size_t i = 0; i < blank_slide_layouts.size(); ++i) {
    content_types += fmt::format(
        R"(<Override PartName="/ppt/slideLayouts/slideLayout{}.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>)",
        i + 1);
  }
  content_types += "</Types>";
  result.emplace_back("[Content_Types].xml", std::move(content_types));

  result.emplace_back("_rels/.rels",
                      relationships(std::array{
                          Target{"officeDocument", "ppt/presentation.xml"},
                          Target{"extended-properties", "docProps/app.xml"},
                      }));
  result.emplace_back(app_part.path, app_part.content);

  // [ECMA-376] 19.2.1.34: a master id is at least 2^31, and the layout ids
  // that follow it are unique across masters and layouts
  result.emplace_back(
      "ppt/presentation.xml",
      fmt::format(
          R"({}<p:presentation{} saveSubsetFonts="1">)"
          R"(<p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>)"
          R"(<p:sldIdLst><p:sldId id="256" r:id="rId2"/></p:sldIdLst>)"
          R"(<p:sldSz cx="{}" cy="{}"/><p:notesSz cx="6858000" cy="9144000"/>)"
          R"(</p:presentation>)",
          xml_declaration, presentation_namespaces, emus(blank_slide_width),
          emus(blank_slide_height)));
  result.emplace_back(
      "ppt/_rels/presentation.xml.rels",
      relationships(std::array{
          Target{"slideMaster", "slideMasters/slideMaster1.xml"},
          Target{"slide", "slides/slide1.xml"},
          Target{"theme", "theme/theme1.xml"},
          Target{"presProps", "presProps.xml"},
      }));
  result.emplace_back("ppt/presProps.xml",
                      fmt::format("{}<p:presentationPr{}/>", xml_declaration,
                                  presentation_namespaces));
  result.emplace_back("ppt/theme/theme1.xml", std::string(theme));

  std::string master =
      fmt::format("{}<p:sldMaster{}><p:cSld>{}{}</p:spTree></p:cSld>",
                  xml_declaration, presentation_namespaces, shape_tree_head,
                  placeholder_shapes(blank_master_placeholders, Owner::master));
  master +=
      R"(<p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>)"
      "<p:sldLayoutIdLst>";
  std::vector<Target> master_targets;
  for (std::size_t i = 0; i < blank_slide_layouts.size(); ++i) {
    master += fmt::format(R"(<p:sldLayoutId id="{}" r:id="rId{}"/>)",
                          2147483649 + i, i + 1);
    master_targets.emplace_back(
        "slideLayout", fmt::format("../slideLayouts/slideLayout{}.xml", i + 1));
  }
  master += "</p:sldLayoutIdLst>";
  master += text_styles;
  master += "</p:sldMaster>";
  master_targets.emplace_back("theme", "../theme/theme1.xml");
  result.emplace_back("ppt/slideMasters/slideMaster1.xml", std::move(master));
  result.emplace_back("ppt/slideMasters/_rels/slideMaster1.xml.rels",
                      relationships(master_targets));

  for (std::size_t i = 0; i < blank_slide_layouts.size(); ++i) {
    const BlankSlideLayout &layout = blank_slide_layouts[i];
    result.emplace_back(
        fmt::format("ppt/slideLayouts/slideLayout{}.xml", i + 1),
        fmt::format(
            R"({}<p:sldLayout{} type="{}" preserve="1"><p:cSld name="{}">{}{}</p:spTree></p:cSld>)"
            R"(<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>)",
            xml_declaration, presentation_namespaces, layout.ooxml_type,
            layout.name, shape_tree_head,
            placeholder_shapes(layout.placeholders, Owner::layout)));
    result.emplace_back(
        fmt::format("ppt/slideLayouts/_rels/slideLayout{}.xml.rels", i + 1),
        relationships(std::array{
            Target{"slideMaster", "../slideMasters/slideMaster1.xml"}}));
  }

  result.emplace_back(
      "ppt/slides/slide1.xml",
      fmt::format(R"({}<p:sld{}><p:cSld>{}{}</p:spTree></p:cSld>)"
                  R"(<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>)",
                  xml_declaration, presentation_namespaces, shape_tree_head,
                  placeholder_shapes(blank_slide_layouts[0].placeholders,
                                     Owner::slide)));
  result.emplace_back("ppt/slides/_rels/slide1.xml.rels",
                      relationships(std::array{Target{
                          "slideLayout", "../slideLayouts/slideLayout1.xml"}}));

  return result;
}

std::vector<std::pair<std::string, std::string>>
find_parts(const FileType type) {
  const auto copy = [](const std::span<const Part> parts) {
    std::vector<std::pair<std::string, std::string>> result;
    for (const Part &part : parts) {
      result.emplace_back(part.path, part.content);
    }
    return result;
  };
  switch (type) {
  case FileType::office_open_xml_document:
    return copy(document_parts);
  case FileType::office_open_xml_presentation:
    return presentation_parts();
  case FileType::office_open_xml_workbook:
    return copy(workbook_parts);
  default:
    throw UnsupportedFileType(type);
  }
}

} // namespace

std::string blank_package(const FileType type) {
  zip::ZipArchive archive;
  for (auto &[path, content] : find_parts(type)) {
    archive.insert_file(std::end(archive), RelPath(path),
                        std::make_shared<MemoryFile>(std::move(content)));
  }

  std::stringstream out;
  archive.save(out);
  return std::move(out).str();
}

} // namespace odr::internal::ooxml
