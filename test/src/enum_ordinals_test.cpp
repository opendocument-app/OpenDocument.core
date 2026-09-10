// Every enum here crosses a binding by ordinal: JNI resolves constants with
// `values()[code]` (`jni_style.cpp:91`), and the apple and wasm mirrors are
// positional too. Appending is silent; inserting or reordering fails here.
// `wasm/tests/enums.test.mjs` does the same on the JS side.

#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/font.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <gtest/gtest.h>

using namespace odr;

namespace {

template <typename Enum> constexpr int ordinal(const Enum value) {
  return static_cast<int>(value);
}

} // namespace

TEST(EnumOrdinals, file_type) {
  EXPECT_EQ(ordinal(FileType::unknown), 0);
  EXPECT_EQ(ordinal(FileType::opendocument_text), 1);
  EXPECT_EQ(ordinal(FileType::opendocument_presentation), 2);
  EXPECT_EQ(ordinal(FileType::opendocument_spreadsheet), 3);
  EXPECT_EQ(ordinal(FileType::opendocument_graphics), 4);
  EXPECT_EQ(ordinal(FileType::office_open_xml_document), 5);
  EXPECT_EQ(ordinal(FileType::office_open_xml_presentation), 6);
  EXPECT_EQ(ordinal(FileType::office_open_xml_workbook), 7);
  EXPECT_EQ(ordinal(FileType::office_open_xml_encrypted), 8);
  EXPECT_EQ(ordinal(FileType::excel_binary_workbook), 9);
  EXPECT_EQ(ordinal(FileType::legacy_word_document), 10);
  EXPECT_EQ(ordinal(FileType::legacy_powerpoint_presentation), 11);
  EXPECT_EQ(ordinal(FileType::legacy_excel_worksheets), 12);
  EXPECT_EQ(ordinal(FileType::word_perfect), 13);
  EXPECT_EQ(ordinal(FileType::rich_text_format), 14);
  EXPECT_EQ(ordinal(FileType::portable_document_format), 15);
  EXPECT_EQ(ordinal(FileType::text_file), 16);
  EXPECT_EQ(ordinal(FileType::comma_separated_values), 17);
  EXPECT_EQ(ordinal(FileType::javascript_object_notation), 18);
  EXPECT_EQ(ordinal(FileType::markdown), 19);
  EXPECT_EQ(ordinal(FileType::zip), 20);
  EXPECT_EQ(ordinal(FileType::compound_file_binary_format), 21);
  EXPECT_EQ(ordinal(FileType::portable_network_graphics), 22);
  EXPECT_EQ(ordinal(FileType::graphics_interchange_format), 23);
  EXPECT_EQ(ordinal(FileType::jpeg), 24);
  EXPECT_EQ(ordinal(FileType::bitmap_image_file), 25);
  EXPECT_EQ(ordinal(FileType::starview_metafile), 26);
  EXPECT_EQ(ordinal(FileType::truetype_font), 27);
  EXPECT_EQ(ordinal(FileType::opentype_font), 28);
  EXPECT_EQ(ordinal(FileType::webp), 29);
  EXPECT_EQ(ordinal(FileType::tagged_image_file_format), 30);
  EXPECT_EQ(ordinal(FileType::high_efficiency_image_format), 31);
  EXPECT_EQ(ordinal(FileType::av1_image_file_format), 32);
  EXPECT_EQ(ordinal(FileType::mpeg_audio), 33);
  EXPECT_EQ(ordinal(FileType::mpeg4_audio), 34);
  EXPECT_EQ(ordinal(FileType::ogg_audio), 35);
  EXPECT_EQ(ordinal(FileType::waveform_audio), 36);
  EXPECT_EQ(ordinal(FileType::free_lossless_audio_codec), 37);
  EXPECT_EQ(ordinal(FileType::mpeg4_video), 38);
  EXPECT_EQ(ordinal(FileType::quicktime_video), 39);
  EXPECT_EQ(ordinal(FileType::third_generation_partnership_video), 40);
  EXPECT_EQ(ordinal(FileType::matroska_video), 41);
  EXPECT_EQ(ordinal(FileType::audio_video_interleave), 42);
  EXPECT_EQ(ordinal(FileType::scalable_vector_graphics), 43);
  EXPECT_EQ(ordinal(FileType::windows_icon), 44);
  EXPECT_EQ(ordinal(FileType::jpeg_xl), 45);
  EXPECT_EQ(ordinal(FileType::jpeg_2000), 46);
  EXPECT_EQ(ordinal(FileType::photoshop_document), 47);
  EXPECT_EQ(ordinal(FileType::windows_metafile), 48);
  EXPECT_EQ(ordinal(FileType::enhanced_metafile), 49);
  EXPECT_EQ(ordinal(FileType::xml), 50);
  EXPECT_EQ(ordinal(FileType::iwork_pages), 51);
  EXPECT_EQ(ordinal(FileType::iwork_numbers), 52);
  EXPECT_EQ(ordinal(FileType::iwork_keynote), 53);
  EXPECT_EQ(ordinal(FileType::hypertext_markup_language), 54);
}

TEST(EnumOrdinals, file_category) {
  EXPECT_EQ(ordinal(FileCategory::unknown), 0);
  EXPECT_EQ(ordinal(FileCategory::text), 1);
  EXPECT_EQ(ordinal(FileCategory::image), 2);
  EXPECT_EQ(ordinal(FileCategory::archive), 3);
  EXPECT_EQ(ordinal(FileCategory::document), 4);
  EXPECT_EQ(ordinal(FileCategory::font), 5);
  EXPECT_EQ(ordinal(FileCategory::audio), 6);
  EXPECT_EQ(ordinal(FileCategory::video), 7);
}

TEST(EnumOrdinals, file_location) {
  EXPECT_EQ(ordinal(FileLocation::unknown), 0);
  EXPECT_EQ(ordinal(FileLocation::memory), 1);
  EXPECT_EQ(ordinal(FileLocation::disk), 2);
}

TEST(EnumOrdinals, encryption_state) {
  EXPECT_EQ(ordinal(EncryptionState::unknown), 0);
  EXPECT_EQ(ordinal(EncryptionState::not_encrypted), 1);
  EXPECT_EQ(ordinal(EncryptionState::encrypted), 2);
  EXPECT_EQ(ordinal(EncryptionState::decrypted), 3);
}

TEST(EnumOrdinals, document_type) {
  EXPECT_EQ(ordinal(DocumentType::unknown), 0);
  EXPECT_EQ(ordinal(DocumentType::text), 1);
  EXPECT_EQ(ordinal(DocumentType::presentation), 2);
  EXPECT_EQ(ordinal(DocumentType::spreadsheet), 3);
  EXPECT_EQ(ordinal(DocumentType::drawing), 4);
}

TEST(EnumOrdinals, text_encoding) {
  EXPECT_EQ(ordinal(TextEncoding::unknown), 0);
  EXPECT_EQ(ordinal(TextEncoding::utf8), 1);
  EXPECT_EQ(ordinal(TextEncoding::utf16le), 2);
  EXPECT_EQ(ordinal(TextEncoding::utf16be), 3);
  EXPECT_EQ(ordinal(TextEncoding::utf32le), 4);
  EXPECT_EQ(ordinal(TextEncoding::utf32be), 5);
  EXPECT_EQ(ordinal(TextEncoding::ibm866), 6);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_1), 7);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_2), 8);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_3), 9);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_4), 10);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_5), 11);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_6), 12);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_7), 13);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_8), 14);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_10), 15);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_13), 16);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_14), 17);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_15), 18);
  EXPECT_EQ(ordinal(TextEncoding::iso_8859_16), 19);
  EXPECT_EQ(ordinal(TextEncoding::koi8_r), 20);
  EXPECT_EQ(ordinal(TextEncoding::koi8_u), 21);
  EXPECT_EQ(ordinal(TextEncoding::macintosh), 22);
  EXPECT_EQ(ordinal(TextEncoding::windows_874), 23);
  EXPECT_EQ(ordinal(TextEncoding::windows_1250), 24);
  EXPECT_EQ(ordinal(TextEncoding::windows_1251), 25);
  EXPECT_EQ(ordinal(TextEncoding::windows_1252), 26);
  EXPECT_EQ(ordinal(TextEncoding::windows_1253), 27);
  EXPECT_EQ(ordinal(TextEncoding::windows_1254), 28);
  EXPECT_EQ(ordinal(TextEncoding::windows_1255), 29);
  EXPECT_EQ(ordinal(TextEncoding::windows_1256), 30);
  EXPECT_EQ(ordinal(TextEncoding::windows_1257), 31);
  EXPECT_EQ(ordinal(TextEncoding::windows_1258), 32);
  EXPECT_EQ(ordinal(TextEncoding::x_mac_cyrillic), 33);
  EXPECT_EQ(ordinal(TextEncoding::big5), 34);
  EXPECT_EQ(ordinal(TextEncoding::euc_jp), 35);
  EXPECT_EQ(ordinal(TextEncoding::euc_kr), 36);
  EXPECT_EQ(ordinal(TextEncoding::gb18030), 37);
  EXPECT_EQ(ordinal(TextEncoding::iso_2022_jp), 38);
  EXPECT_EQ(ordinal(TextEncoding::iso_2022_kr), 39);
  EXPECT_EQ(ordinal(TextEncoding::shift_jis), 40);
}

TEST(EnumOrdinals, element_type) {
  EXPECT_EQ(ordinal(ElementType::none), 0);
  EXPECT_EQ(ordinal(ElementType::root), 1);
  EXPECT_EQ(ordinal(ElementType::slide), 2);
  EXPECT_EQ(ordinal(ElementType::sheet), 3);
  EXPECT_EQ(ordinal(ElementType::page), 4);
  EXPECT_EQ(ordinal(ElementType::master_page), 5);
  EXPECT_EQ(ordinal(ElementType::sheet_cell), 6);
  EXPECT_EQ(ordinal(ElementType::text), 7);
  EXPECT_EQ(ordinal(ElementType::line_break), 8);
  EXPECT_EQ(ordinal(ElementType::page_break), 9);
  EXPECT_EQ(ordinal(ElementType::paragraph), 10);
  EXPECT_EQ(ordinal(ElementType::span), 11);
  EXPECT_EQ(ordinal(ElementType::link), 12);
  EXPECT_EQ(ordinal(ElementType::bookmark), 13);
  EXPECT_EQ(ordinal(ElementType::list), 14);
  EXPECT_EQ(ordinal(ElementType::list_item), 15);
  EXPECT_EQ(ordinal(ElementType::table), 16);
  EXPECT_EQ(ordinal(ElementType::table_column), 17);
  EXPECT_EQ(ordinal(ElementType::table_row), 18);
  EXPECT_EQ(ordinal(ElementType::table_cell), 19);
  EXPECT_EQ(ordinal(ElementType::frame), 20);
  EXPECT_EQ(ordinal(ElementType::image), 21);
  EXPECT_EQ(ordinal(ElementType::group), 22);
}

TEST(EnumOrdinals, shape_type) {
  EXPECT_EQ(ordinal(ShapeType::none), 0);
  EXPECT_EQ(ordinal(ShapeType::rect), 1);
  EXPECT_EQ(ordinal(ShapeType::ellipse), 2);
  EXPECT_EQ(ordinal(ShapeType::line), 3);
  EXPECT_EQ(ordinal(ShapeType::custom), 4);
}

TEST(EnumOrdinals, anchor_type) {
  EXPECT_EQ(ordinal(AnchorType::none), 0);
  EXPECT_EQ(ordinal(AnchorType::as_char), 1);
  EXPECT_EQ(ordinal(AnchorType::at_char), 2);
  EXPECT_EQ(ordinal(AnchorType::at_frame), 3);
  EXPECT_EQ(ordinal(AnchorType::at_page), 4);
  EXPECT_EQ(ordinal(AnchorType::at_paragraph), 5);
}

TEST(EnumOrdinals, value_type) {
  EXPECT_EQ(ordinal(ValueType::unknown), 0);
  EXPECT_EQ(ordinal(ValueType::string), 1);
  EXPECT_EQ(ordinal(ValueType::float_number), 2);
}

TEST(EnumOrdinals, list_type) {
  EXPECT_EQ(ordinal(ListType::unordered), 0);
  EXPECT_EQ(ordinal(ListType::ordered), 1);
}

TEST(EnumOrdinals, font_format) {
  EXPECT_EQ(ordinal(FontFormat::unknown), 0);
  EXPECT_EQ(ordinal(FontFormat::truetype), 1);
  EXPECT_EQ(ordinal(FontFormat::opentype_cff), 2);
  EXPECT_EQ(ordinal(FontFormat::cff), 3);
  EXPECT_EQ(ordinal(FontFormat::type1), 4);
}

TEST(EnumOrdinals, html_resource_type) {
  EXPECT_EQ(ordinal(HtmlResourceType::html_fragment), 0);
  EXPECT_EQ(ordinal(HtmlResourceType::css), 1);
  EXPECT_EQ(ordinal(HtmlResourceType::js), 2);
  EXPECT_EQ(ordinal(HtmlResourceType::image), 3);
  EXPECT_EQ(ordinal(HtmlResourceType::font), 4);
  EXPECT_EQ(ordinal(HtmlResourceType::media), 5);
  EXPECT_EQ(ordinal(HtmlResourceType::file), 6);
}

TEST(EnumOrdinals, html_table_gridlines) {
  EXPECT_EQ(ordinal(HtmlTableGridlines::none), 0);
  EXPECT_EQ(ordinal(HtmlTableGridlines::soft), 1);
  EXPECT_EQ(ordinal(HtmlTableGridlines::hard), 2);
}

TEST(EnumOrdinals, html_color_scheme) {
  EXPECT_EQ(ordinal(HtmlColorScheme::light), 0);
  EXPECT_EQ(ordinal(HtmlColorScheme::dark), 1);
  EXPECT_EQ(ordinal(HtmlColorScheme::system), 2);
}

TEST(EnumOrdinals, html_viewport_mode) {
  EXPECT_EQ(ordinal(HtmlViewportMode::automatic), 0);
  EXPECT_EQ(ordinal(HtmlViewportMode::fit_width), 1);
  EXPECT_EQ(ordinal(HtmlViewportMode::actual_size), 2);
  EXPECT_EQ(ordinal(HtmlViewportMode::none), 3);
  EXPECT_EQ(ordinal(HtmlViewportMode::fit_width_by_view), 4);
}

TEST(EnumOrdinals, pdf_text_mode) {
  EXPECT_EQ(ordinal(PdfTextMode::dual_layer), 0);
  EXPECT_EQ(ordinal(PdfTextMode::single_layer), 1);
}

TEST(EnumOrdinals, log_level) {
  EXPECT_EQ(ordinal(LogLevel::verbose), 0);
  EXPECT_EQ(ordinal(LogLevel::debug), 1);
  EXPECT_EQ(ordinal(LogLevel::info), 2);
  EXPECT_EQ(ordinal(LogLevel::warning), 3);
  EXPECT_EQ(ordinal(LogLevel::error), 4);
  EXPECT_EQ(ordinal(LogLevel::fatal), 5);
}

TEST(EnumOrdinals, font_weight) {
  EXPECT_EQ(ordinal(FontWeight::normal), 0);
  EXPECT_EQ(ordinal(FontWeight::bold), 1);
}

TEST(EnumOrdinals, font_style) {
  EXPECT_EQ(ordinal(FontStyle::normal), 0);
  EXPECT_EQ(ordinal(FontStyle::italic), 1);
}

TEST(EnumOrdinals, font_position) {
  EXPECT_EQ(ordinal(FontPosition::normal), 0);
  EXPECT_EQ(ordinal(FontPosition::super), 1);
  EXPECT_EQ(ordinal(FontPosition::sub), 2);
}

TEST(EnumOrdinals, text_align) {
  EXPECT_EQ(ordinal(TextAlign::left), 0);
  EXPECT_EQ(ordinal(TextAlign::right), 1);
  EXPECT_EQ(ordinal(TextAlign::center), 2);
  EXPECT_EQ(ordinal(TextAlign::justify), 3);
  EXPECT_EQ(ordinal(TextAlign::start), 4);
  EXPECT_EQ(ordinal(TextAlign::end), 5);
}

TEST(EnumOrdinals, text_direction) {
  EXPECT_EQ(ordinal(TextDirection::left_to_right), 0);
  EXPECT_EQ(ordinal(TextDirection::right_to_left), 1);
}

TEST(EnumOrdinals, horizontal_align) {
  EXPECT_EQ(ordinal(HorizontalAlign::left), 0);
  EXPECT_EQ(ordinal(HorizontalAlign::center), 1);
  EXPECT_EQ(ordinal(HorizontalAlign::right), 2);
}

TEST(EnumOrdinals, vertical_align) {
  EXPECT_EQ(ordinal(VerticalAlign::top), 0);
  EXPECT_EQ(ordinal(VerticalAlign::middle), 1);
  EXPECT_EQ(ordinal(VerticalAlign::bottom), 2);
}

TEST(EnumOrdinals, break_type) {
  EXPECT_EQ(ordinal(BreakType::none), 0);
  EXPECT_EQ(ordinal(BreakType::page), 1);
  EXPECT_EQ(ordinal(BreakType::column), 2);
}

TEST(EnumOrdinals, print_orientation) {
  EXPECT_EQ(ordinal(PrintOrientation::portrait), 0);
  EXPECT_EQ(ordinal(PrintOrientation::landscape), 1);
}

TEST(EnumOrdinals, text_wrap) {
  EXPECT_EQ(ordinal(TextWrap::none), 0);
  EXPECT_EQ(ordinal(TextWrap::before), 1);
  EXPECT_EQ(ordinal(TextWrap::after), 2);
  EXPECT_EQ(ordinal(TextWrap::run_through), 3);
}
