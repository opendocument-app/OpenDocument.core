#pragma once

#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <jni.h>

#include <optional>
#include <string>
#include <string_view>

namespace odr_jni {

// C++ to Java; optionals map to null.
jstring make_string_opt(JNIEnv *env, const std::optional<std::string> &value);
jstring make_string_opt(JNIEnv *env,
                        const std::optional<std::string_view> &value);
jobject make_integer_opt(JNIEnv *env, const std::optional<std::int32_t> &value);
jobject make_measure(JNIEnv *env, const odr::Measure &value);
jobject make_measure(JNIEnv *env, const std::optional<odr::Measure> &value);
jobject make_color(JNIEnv *env, const std::optional<odr::Color> &value);
jobject make_directional_measure(JNIEnv *env,
                                 const odr::DirectionalStyle<odr::Measure> &);
jobject make_directional_string(JNIEnv *env,
                                const odr::DirectionalStyle<std::string> &);
jobject make_text_style(JNIEnv *env, const odr::TextStyle &style);
jobject make_paragraph_style(JNIEnv *env, const odr::ParagraphStyle &style);
jobject make_table_style(JNIEnv *env, const odr::TableStyle &style);
jobject make_table_column_style(JNIEnv *env,
                                const odr::TableColumnStyle &style);
jobject make_table_row_style(JNIEnv *env, const odr::TableRowStyle &style);
jobject make_table_cell_style(JNIEnv *env, const odr::TableCellStyle &style);
jobject make_graphic_style(JNIEnv *env, const odr::GraphicStyle &style);
jobject make_page_layout(JNIEnv *env, const odr::PageLayout &layout);
jobject make_table_dimensions(JNIEnv *env,
                              const odr::TableDimensions &dimensions);
jobject make_table_position(JNIEnv *env, const odr::TablePosition &position);
jobject make_file_meta(JNIEnv *env, const odr::FileMeta &meta);

jobject html_config_to_java(JNIEnv *env, const odr::HtmlConfig &config);
odr::HtmlConfig html_config_from_java(JNIEnv *env, jobject config);

/// Optional enum to a Java-side code; -1 encodes absent.
template <typename E> jint enum_code(const std::optional<E> &value) {
  return value.has_value() ? static_cast<jint>(*value) : jint{-1};
}

} // namespace odr_jni
