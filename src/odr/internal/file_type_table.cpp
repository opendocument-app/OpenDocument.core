#include <odr/internal/file_type_table.hpp>

#include <algorithm>
#include <array>

namespace odr::internal {

namespace {

using file_type_table::Row;

using namespace std::string_view_literals;

// File extensions and MIME types accepted for each file type. The first entry
// of a list is the canonical one. Aliases have to stay unique across file
// types — the lookups take the first match and `odr_test` asserts that no
// extension or MIME type appears twice.

constexpr std::array odt_extensions{"odt"sv, "fodt"sv, "ott"sv, "odm"sv,
                                    "otm"sv};
constexpr std::array odt_mimetypes{
    "application/vnd.oasis.opendocument.text"sv,
    "application/x-vnd.oasis.opendocument.text"sv,
    "application/vnd.oasis.opendocument.text-template"sv,
    "application/vnd.oasis.opendocument.text-master"sv,
    "application/vnd.oasis.opendocument.text-master-template"sv,
    "application/vnd.oasis.opendocument.text-flat-xml"sv,
};

constexpr std::array odp_extensions{"odp"sv, "fodp"sv, "otp"sv};
constexpr std::array odp_mimetypes{
    "application/vnd.oasis.opendocument.presentation"sv,
    "application/x-vnd.oasis.opendocument.presentation"sv,
    "application/vnd.oasis.opendocument.presentation-template"sv,
    "application/vnd.oasis.opendocument.presentation-flat-xml"sv,
};

constexpr std::array ods_extensions{"ods"sv, "fods"sv, "ots"sv};
constexpr std::array ods_mimetypes{
    "application/vnd.oasis.opendocument.spreadsheet"sv,
    "application/x-vnd.oasis.opendocument.spreadsheet"sv,
    "application/vnd.oasis.opendocument.spreadsheet-template"sv,
    "application/vnd.oasis.opendocument.spreadsheet-flat-xml"sv,
};

constexpr std::array odg_extensions{"odg"sv, "fodg"sv, "otg"sv};
constexpr std::array odg_mimetypes{
    "application/vnd.oasis.opendocument.graphics"sv,
    "application/x-vnd.oasis.opendocument.graphics"sv,
    "application/vnd.oasis.opendocument.graphics-template"sv,
    "application/vnd.oasis.opendocument.graphics-flat-xml"sv,
};

constexpr std::array docx_extensions{"docx"sv, "docm"sv, "dotx"sv, "dotm"sv};
constexpr std::array docx_mimetypes{
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document"sv,
    "application/vnd.openxmlformats-officedocument.wordprocessingml.template"sv,
    "application/vnd.ms-word.document.macroEnabled.12"sv,
    "application/vnd.ms-word.template.macroEnabled.12"sv,
};

constexpr std::array pptx_extensions{"pptx"sv, "pptm"sv, "potx"sv,
                                     "potm"sv, "ppsx"sv, "ppsm"sv};
constexpr std::array pptx_mimetypes{
    "application/"
    "vnd.openxmlformats-officedocument.presentationml.presentation"sv,
    "application/vnd.openxmlformats-officedocument.presentationml.slideshow"sv,
    "application/vnd.openxmlformats-officedocument.presentationml.template"sv,
    "application/vnd.ms-powerpoint.presentation.macroEnabled.12"sv,
    "application/vnd.ms-powerpoint.slideshow.macroEnabled.12"sv,
    "application/vnd.ms-powerpoint.template.macroEnabled.12"sv,
};

// `xlsb` is a binary package, not an OOXML one — we classify it here because
// that is its MIME family, but there is no decoder for it. Detection and
// opening genuinely differ; see `FileTypeCapabilities`.
constexpr std::array xlsx_extensions{"xlsx"sv, "xlsm"sv, "xltx"sv, "xltm"sv,
                                     "xlsb"sv};
constexpr std::array xlsx_mimetypes{
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"sv,
    "application/vnd.openxmlformats-officedocument.spreadsheetml.template"sv,
    "application/vnd.ms-excel.sheet.macroEnabled.12"sv,
    "application/vnd.ms-excel.template.macroEnabled.12"sv,
    "application/vnd.ms-excel.sheet.binary.macroEnabled.12"sv,
};

// An encrypted OOXML package is carried by an ordinary `docx`/`pptx`/`xlsx`
// file, so it has no extension of its own.
constexpr std::array ooxml_encrypted_mimetypes{"application/vnd.ms-office"sv};

constexpr std::array doc_extensions{"doc"sv, "dot"sv};
constexpr std::array doc_mimetypes{"application/msword"sv,
                                   "application/vnd.ms-word"sv};

constexpr std::array ppt_extensions{"ppt"sv, "pot"sv, "pps"sv};
constexpr std::array ppt_mimetypes{"application/vnd.ms-powerpoint"sv,
                                   "application/mspowerpoint"sv};

constexpr std::array xls_extensions{"xls"sv, "xlt"sv, "xlm"sv};
constexpr std::array xls_mimetypes{"application/vnd.ms-excel"sv,
                                   "application/msexcel"sv};

constexpr std::array wpd_extensions{"wpd"sv};
constexpr std::array wpd_mimetypes{"application/vnd.wordperfect"sv,
                                   "application/wordperfect"sv,
                                   "application/x-wordperfect"sv};

constexpr std::array rtf_extensions{"rtf"sv};
constexpr std::array rtf_mimetypes{"application/rtf"sv, "text/rtf"sv,
                                   "application/x-rtf"sv};

constexpr std::array pdf_extensions{"pdf"sv};
constexpr std::array pdf_mimetypes{"application/pdf"sv, "application/x-pdf"sv};

constexpr std::array txt_extensions{"txt"sv, "text"sv};
constexpr std::array txt_mimetypes{"text/plain"sv};

constexpr std::array csv_extensions{"csv"sv};
constexpr std::array csv_mimetypes{"text/csv"sv, "application/csv"sv,
                                   "text/comma-separated-values"sv};

constexpr std::array json_extensions{"json"sv};
constexpr std::array json_mimetypes{"application/json"sv, "text/json"sv};

constexpr std::array markdown_extensions{"md"sv, "markdown"sv};
constexpr std::array markdown_mimetypes{"text/markdown"sv, "text/x-markdown"sv};

constexpr std::array zip_extensions{"zip"sv};
constexpr std::array zip_mimetypes{
    "application/zip"sv, "application/x-zip-compressed"sv, "multipart/x-zip"sv};

constexpr std::array cfb_extensions{"cfb"sv};
constexpr std::array cfb_mimetypes{"application/x-cfb"sv,
                                   "application/x-ole-storage"sv};

constexpr std::array png_extensions{"png"sv};
constexpr std::array png_mimetypes{"image/png"sv};

constexpr std::array gif_extensions{"gif"sv};
constexpr std::array gif_mimetypes{"image/gif"sv};

constexpr std::array jpg_extensions{"jpg"sv, "jpeg"sv, "jpe"sv,
                                    "jif"sv, "jfif"sv, "jfi"sv};
constexpr std::array jpg_mimetypes{"image/jpeg"sv, "image/jpg"sv};

constexpr std::array bmp_extensions{"bmp"sv, "dib"sv};
constexpr std::array bmp_mimetypes{"image/bmp"sv, "image/x-ms-bmp"sv,
                                   "image/x-bmp"sv};

constexpr std::array svm_extensions{"svm"sv};
constexpr std::array svm_mimetypes{"application/x-starview-metafile"sv};

constexpr std::array ttf_extensions{"ttf"sv, "ttc"sv};
constexpr std::array ttf_mimetypes{"font/ttf"sv, "application/x-font-ttf"sv,
                                   "application/x-font-truetype"sv,
                                   "font/collection"sv};

constexpr std::array otf_extensions{"otf"sv};
constexpr std::array otf_mimetypes{"font/otf"sv, "application/x-font-otf"sv,
                                   "application/x-font-opentype"sv,
                                   "application/vnd.ms-opentype"sv};

// The single source of truth behind every public format lookup. `odr_test`
// asserts that it covers each `FileType` exactly once and that the capability
// bits agree with what the engines actually do.
//
// `decrypt` on the OOXML document types refers to a password-protected
// package, which is detected as `office_open_xml_encrypted` and decrypts into
// the type named here. ODF files decrypt in place and keep their type.
constexpr std::array table{
    Row{FileType::unknown,
        "unknown"sv,
        {},
        {},
        FileCategory::unknown,
        DocumentType::unknown,
        {}},

    Row{FileType::opendocument_text,
        "odt"sv,
        odt_extensions,
        odt_mimetypes,
        FileCategory::document,
        DocumentType::text,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true,
         .edit = true,
         .save = true}},
    Row{FileType::opendocument_presentation,
        "odp"sv,
        odp_extensions,
        odp_mimetypes,
        FileCategory::document,
        DocumentType::presentation,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true,
         .edit = true,
         .save = true}},
    // ODF spreadsheet editing is force-disabled, see `odf::Document`.
    Row{FileType::opendocument_spreadsheet,
        "ods"sv,
        ods_extensions,
        ods_mimetypes,
        FileCategory::document,
        DocumentType::spreadsheet,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true,
         .save = true}},
    Row{FileType::opendocument_graphics,
        "odg"sv,
        odg_extensions,
        odg_mimetypes,
        FileCategory::document,
        DocumentType::drawing,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true,
         .edit = true,
         .save = true}},

    Row{FileType::office_open_xml_document,
        "docx"sv,
        docx_extensions,
        docx_mimetypes,
        FileCategory::document,
        DocumentType::text,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true,
         .edit = true,
         .save = true}},
    Row{FileType::office_open_xml_presentation,
        "pptx"sv,
        pptx_extensions,
        pptx_mimetypes,
        FileCategory::document,
        DocumentType::presentation,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true}},
    Row{FileType::office_open_xml_workbook,
        "xlsx"sv,
        xlsx_extensions,
        xlsx_mimetypes,
        FileCategory::document,
        DocumentType::spreadsheet,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true}},
    Row{FileType::office_open_xml_encrypted,
        "ooxml_encrypted"sv,
        {},
        ooxml_encrypted_mimetypes,
        FileCategory::document,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .decrypt = true}},

    Row{FileType::legacy_word_document,
        "doc"sv,
        doc_extensions,
        doc_mimetypes,
        FileCategory::document,
        DocumentType::text,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::legacy_powerpoint_presentation,
        "ppt"sv,
        ppt_extensions,
        ppt_mimetypes,
        FileCategory::document,
        DocumentType::presentation,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::legacy_excel_worksheets,
        "xls"sv,
        xls_extensions,
        xls_mimetypes,
        FileCategory::document,
        DocumentType::spreadsheet,
        {.detect_by_content = true, .open = true, .translate_html = true}},

    // Recognised by magic so a caller can name the type, but there is no
    // decoder behind either of these.
    Row{FileType::word_perfect,
        "wpd"sv,
        wpd_extensions,
        wpd_mimetypes,
        FileCategory::document,
        DocumentType::unknown,
        {.detect_by_content = true}},
    Row{FileType::rich_text_format,
        "rtf"sv,
        rtf_extensions,
        rtf_mimetypes,
        FileCategory::document,
        DocumentType::unknown,
        {.detect_by_content = true}},

    Row{FileType::portable_document_format,
        "pdf"sv,
        pdf_extensions,
        pdf_mimetypes,
        FileCategory::document,
        DocumentType::text,
        {.detect_by_content = true,
         .open = true,
         .decrypt = true,
         .translate_html = true}},

    Row{FileType::text_file,
        "txt"sv,
        txt_extensions,
        txt_mimetypes,
        FileCategory::text,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::comma_separated_values,
        "csv"sv,
        csv_extensions,
        csv_mimetypes,
        FileCategory::text,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::javascript_object_notation,
        "json"sv,
        json_extensions,
        json_mimetypes,
        FileCategory::text,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    // Classified for callers that route files by type; there is no decoder.
    Row{FileType::markdown,
        "md"sv,
        markdown_extensions,
        markdown_mimetypes,
        FileCategory::text,
        DocumentType::unknown,
        {}},

    Row{FileType::zip,
        "zip"sv,
        zip_extensions,
        zip_mimetypes,
        FileCategory::archive,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::compound_file_binary_format,
        "cfb"sv,
        cfb_extensions,
        cfb_mimetypes,
        FileCategory::archive,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},

    Row{FileType::portable_network_graphics,
        "png"sv,
        png_extensions,
        png_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::graphics_interchange_format,
        "gif"sv,
        gif_extensions,
        gif_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::jpeg,
        "jpg"sv,
        jpg_extensions,
        jpg_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::bitmap_image_file,
        "bmp"sv,
        bmp_extensions,
        bmp_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::starview_metafile,
        "svm"sv,
        svm_extensions,
        svm_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},

    Row{FileType::truetype_font,
        "ttf"sv,
        ttf_extensions,
        ttf_mimetypes,
        FileCategory::font,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::opentype_font,
        "otf"sv,
        otf_extensions,
        otf_mimetypes,
        FileCategory::font,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
};

/// Finds the row whose list, selected by @p list, contains @p needle.
template <typename Projection>
const Row *find_by_alias(const std::string_view needle,
                         Projection list) noexcept {
  const auto it = std::ranges::find_if(table, [&](const Row &row) {
    return std::ranges::find(list(row), needle) != std::ranges::end(list(row));
  });
  return it == std::ranges::end(table) ? nullptr : &*it;
}

} // namespace

std::span<const file_type_table::Row> file_type_table::rows() noexcept {
  return table;
}

const file_type_table::Row *
file_type_table::find(const FileType type) noexcept {
  const auto it = std::ranges::find(table, type, &Row::type);
  return it == std::ranges::end(table) ? nullptr : &*it;
}

const file_type_table::Row *
file_type_table::find_by_extension(const std::string_view extension) noexcept {
  return find_by_alias(extension,
                       [](const Row &row) { return row.extensions; });
}

const file_type_table::Row *
file_type_table::find_by_mimetype(const std::string_view mimetype) noexcept {
  return find_by_alias(mimetype, [](const Row &row) { return row.mimetypes; });
}

} // namespace odr::internal
