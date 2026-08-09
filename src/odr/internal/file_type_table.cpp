#include <odr/internal/file_type_table.hpp>

#include <algorithm>
#include <array>

namespace odr::internal {

namespace {

using file_type_table::Row;

using namespace std::string_view_literals;

// Extensions and MIME types per file type, canonical one first. Aliases are
// unique across types — the lookups take the first match, `odr_test` asserts
// no alias appears twice.

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

constexpr std::array xlsx_extensions{"xlsx"sv, "xlsm"sv, "xltx"sv, "xltm"sv};
constexpr std::array xlsx_mimetypes{
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"sv,
    "application/vnd.openxmlformats-officedocument.spreadsheetml.template"sv,
    "application/vnd.ms-excel.sheet.macroEnabled.12"sv,
    "application/vnd.ms-excel.template.macroEnabled.12"sv,
};

// `.xlsb` ships in an OOXML package but stores the workbook in binary parts,
// not spreadsheetml, so it is its own type with no capabilities.
constexpr std::array xlsb_extensions{"xlsb"sv};
constexpr std::array xlsb_mimetypes{
    "application/vnd.ms-excel.sheet.binary.macroEnabled.12"sv};

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

constexpr std::array webp_extensions{"webp"sv};
constexpr std::array webp_mimetypes{"image/webp"sv};

constexpr std::array tiff_extensions{"tif"sv, "tiff"sv};
constexpr std::array tiff_mimetypes{"image/tiff"sv, "image/x-tiff"sv};

constexpr std::array heif_extensions{"heic"sv, "heif"sv, "hif"sv};
constexpr std::array heif_mimetypes{"image/heic"sv, "image/heif"sv,
                                    "image/heic-sequence"sv,
                                    "image/heif-sequence"sv};

constexpr std::array avif_extensions{"avif"sv, "avifs"sv};
constexpr std::array avif_mimetypes{"image/avif"sv, "image/avif-sequence"sv};

constexpr std::array xml_extensions{"xml"sv};
constexpr std::array xml_mimetypes{"application/xml"sv, "text/xml"sv};

constexpr std::array svg_extensions{"svg"sv};
constexpr std::array svg_mimetypes{"image/svg+xml"sv};

// `.cur` is the same container with a different resource type, so it rides
// along here rather than becoming a type of its own
constexpr std::array ico_extensions{"ico"sv, "cur"sv};
constexpr std::array ico_mimetypes{"image/vnd.microsoft.icon"sv,
                                   "image/x-icon"sv};

constexpr std::array jxl_extensions{"jxl"sv};
constexpr std::array jxl_mimetypes{"image/jxl"sv};

constexpr std::array jp2_extensions{"jp2"sv, "jpx"sv, "jpf"sv,
                                    "j2k"sv, "jpc"sv, "j2c"sv};
constexpr std::array jp2_mimetypes{"image/jp2"sv, "image/jpx"sv};

// `.psb` is the large document variant and carries the same signature
constexpr std::array psd_extensions{"psd"sv, "psb"sv};
constexpr std::array psd_mimetypes{"image/vnd.adobe.photoshop"sv,
                                   "application/x-photoshop"sv};

constexpr std::array wmf_extensions{"wmf"sv};
constexpr std::array wmf_mimetypes{"image/wmf"sv, "image/x-wmf"sv,
                                   "application/x-msmetafile"sv};

constexpr std::array emf_extensions{"emf"sv};
constexpr std::array emf_mimetypes{"image/emf"sv, "image/x-emf"sv};

constexpr std::array mp3_extensions{"mp3"sv};
constexpr std::array mp3_mimetypes{"audio/mpeg"sv, "audio/mp3"sv,
                                   "audio/x-mpeg"sv};

constexpr std::array m4a_extensions{"m4a"sv, "m4b"sv};
constexpr std::array m4a_mimetypes{"audio/mp4"sv, "audio/x-m4a"sv,
                                   "audio/m4a"sv};

constexpr std::array ogg_extensions{"ogg"sv, "oga"sv, "opus"sv};
constexpr std::array ogg_mimetypes{"audio/ogg"sv, "application/ogg"sv,
                                   "audio/opus"sv};

constexpr std::array wav_extensions{"wav"sv, "wave"sv};
constexpr std::array wav_mimetypes{"audio/wav"sv, "audio/x-wav"sv,
                                   "audio/wave"sv, "audio/vnd.wave"sv};

constexpr std::array flac_extensions{"flac"sv};
constexpr std::array flac_mimetypes{"audio/flac"sv, "audio/x-flac"sv};

constexpr std::array mp4_extensions{"mp4"sv, "m4v"sv};
constexpr std::array mp4_mimetypes{"video/mp4"sv, "video/x-m4v"sv};

constexpr std::array mov_extensions{"mov"sv, "qt"sv};
constexpr std::array mov_mimetypes{"video/quicktime"sv};

constexpr std::array third_gpp_extensions{"3gp"sv, "3g2"sv};
constexpr std::array third_gpp_mimetypes{"video/3gpp"sv, "video/3gpp2"sv,
                                         "audio/3gpp"sv};

// one type for the whole EBML container: matroska and webm are the same bytes
// until the DocType element, which sits deeper than a signature reaches
constexpr std::array mkv_extensions{"mkv"sv, "webm"sv, "mka"sv};
constexpr std::array mkv_mimetypes{"video/x-matroska"sv, "video/webm"sv,
                                   "audio/x-matroska"sv, "audio/webm"sv};

constexpr std::array avi_extensions{"avi"sv};
constexpr std::array avi_mimetypes{"video/x-msvideo"sv, "video/avi"sv,
                                   "video/msvideo"sv};

// The single source of truth behind every public format lookup; `odr_test`
// asserts one row per `FileType` and capabilities that match the engines.
//
// `decrypt` on an OOXML document type means a password-protected package,
// detected as `office_open_xml_encrypted` and decrypting into the type named
// here. ODF files decrypt in place and keep their type.
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
    // classified so a caller can name and route the file; no decoder
    Row{FileType::excel_binary_workbook,
        "xlsb"sv,
        xlsb_extensions,
        xlsb_mimetypes,
        FileCategory::document,
        DocumentType::spreadsheet,
        {}},

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
        DocumentType::spreadsheet,
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

    // Named but not decoded: `open` wraps the bytes and `translate_html` hands
    // them to the browser in an `<img>` or a player. Nothing reads a pixel or
    // a sample.
    Row{FileType::webp,
        "webp"sv,
        webp_extensions,
        webp_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::tagged_image_file_format,
        "tiff"sv,
        tiff_extensions,
        tiff_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::high_efficiency_image_format,
        "heif"sv,
        heif_extensions,
        heif_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::av1_image_file_format,
        "avif"sv,
        avif_extensions,
        avif_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},

    Row{FileType::mpeg_audio,
        "mp3"sv,
        mp3_extensions,
        mp3_mimetypes,
        FileCategory::audio,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::mpeg4_audio,
        "m4a"sv,
        m4a_extensions,
        m4a_mimetypes,
        FileCategory::audio,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::ogg_audio,
        "ogg"sv,
        ogg_extensions,
        ogg_mimetypes,
        FileCategory::audio,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::waveform_audio,
        "wav"sv,
        wav_extensions,
        wav_mimetypes,
        FileCategory::audio,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::free_lossless_audio_codec,
        "flac"sv,
        flac_extensions,
        flac_mimetypes,
        FileCategory::audio,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},

    Row{FileType::mpeg4_video,
        "mp4"sv,
        mp4_extensions,
        mp4_mimetypes,
        FileCategory::video,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::quicktime_video,
        "mov"sv,
        mov_extensions,
        mov_mimetypes,
        FileCategory::video,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::third_generation_partnership_video,
        "3gp"sv,
        third_gpp_extensions,
        third_gpp_mimetypes,
        FileCategory::video,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::matroska_video,
        "mkv"sv,
        mkv_extensions,
        mkv_mimetypes,
        FileCategory::video,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::audio_video_interleave,
        "avi"sv,
        avi_extensions,
        avi_mimetypes,
        FileCategory::video,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},

    // Named but not decoded, like the images above. `translate_html` means the
    // image page is written and the data url labelled, not that every browser
    // paints it.
    Row{FileType::scalable_vector_graphics,
        "svg"sv,
        svg_extensions,
        svg_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::windows_icon,
        "ico"sv,
        ico_extensions,
        ico_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::jpeg_xl,
        "jxl"sv,
        jxl_extensions,
        jxl_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::jpeg_2000,
        "jp2"sv,
        jp2_extensions,
        jp2_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::photoshop_document,
        "psd"sv,
        psd_extensions,
        psd_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::windows_metafile,
        "wmf"sv,
        wmf_extensions,
        wmf_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},
    Row{FileType::enhanced_metafile,
        "emf"sv,
        emf_extensions,
        emf_mimetypes,
        FileCategory::image,
        DocumentType::unknown,
        {.detect_by_content = true, .open = true, .translate_html = true}},

    // Detection reports it, nothing opens it yet - a plain xml file still
    // decodes as text.
    Row{FileType::xml,
        "xml"sv,
        xml_extensions,
        xml_mimetypes,
        FileCategory::text,
        DocumentType::unknown,
        {.detect_by_content = true}},
};

/// Finds the row whose list, selected by @p list, contains @p needle.
template <typename Projection>
const Row *find_by_alias(const std::string_view needle,
                         Projection list) noexcept {
  const auto it = std::ranges::find_if(table, [&](const Row &row) {
    const auto aliases = list(row);
    return std::ranges::find(aliases, needle) != std::ranges::end(aliases);
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
