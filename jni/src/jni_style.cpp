#include "jni_convert.hpp"
#include "odr_jni.hpp"

#include <cstdarg>
#include <vector>

namespace odr_jni {

namespace {

jobject new_object(JNIEnv *env, const char *class_name, const char *signature,
                   ...) {
  jclass cls = env->FindClass(class_name);
  if (cls == nullptr) {
    return nullptr;
  }
  jmethodID ctor = env->GetMethodID(cls, "<init>", signature);
  if (ctor == nullptr) {
    env->DeleteLocalRef(cls);
    return nullptr;
  }
  va_list args;
  va_start(args, signature);
  jobject result = env->NewObjectV(cls, ctor, args);
  va_end(args);
  env->DeleteLocalRef(cls);
  return result;
}

jobject call_static_object(JNIEnv *env, const char *class_name,
                           const char *method, const char *signature, ...) {
  jclass cls = env->FindClass(class_name);
  if (cls == nullptr) {
    return nullptr;
  }
  jmethodID mid = env->GetStaticMethodID(cls, method, signature);
  if (mid == nullptr) {
    env->DeleteLocalRef(cls);
    return nullptr;
  }
  va_list args;
  va_start(args, signature);
  jobject result = env->CallStaticObjectMethodV(cls, mid, args);
  va_end(args);
  env->DeleteLocalRef(cls);
  return result;
}

jobject box_boolean(JNIEnv *env, const std::optional<bool> &value) {
  if (!value.has_value()) {
    return nullptr;
  }
  return call_static_object(env, "java/lang/Boolean", "valueOf",
                            "(Z)Ljava/lang/Boolean;",
                            static_cast<jboolean>(*value));
}

jobject box_double(JNIEnv *env, const std::optional<double> &value) {
  if (!value.has_value()) {
    return nullptr;
  }
  return call_static_object(env, "java/lang/Double", "valueOf",
                            "(D)Ljava/lang/Double;", *value);
}

jobject box_long(JNIEnv *env, const std::optional<std::uint32_t> &value) {
  if (!value.has_value()) {
    return nullptr;
  }
  return call_static_object(env, "java/lang/Long", "valueOf",
                            "(J)Ljava/lang/Long;", static_cast<jlong>(*value));
}

jobject box_integer(JNIEnv *env, const std::optional<std::uint32_t> &value) {
  if (!value.has_value()) {
    return nullptr;
  }
  return call_static_object(env, "java/lang/Integer", "valueOf",
                            "(I)Ljava/lang/Integer;",
                            static_cast<jint>(*value));
}

/// Looks up an enum constant by its C++ code (= Java ordinal).
jobject enum_from_code(JNIEnv *env, const char *class_name, const jint code) {
  if (code < 0) {
    return nullptr;
  }
  jclass cls = env->FindClass(class_name);
  if (cls == nullptr) {
    return nullptr;
  }
  const std::string signature = std::string("()[L") + class_name + ";";
  jmethodID values = env->GetStaticMethodID(cls, "values", signature.c_str());
  if (values == nullptr) {
    env->DeleteLocalRef(cls);
    return nullptr;
  }
  auto array =
      static_cast<jobjectArray>(env->CallStaticObjectMethod(cls, values));
  env->DeleteLocalRef(cls);
  if (array == nullptr) {
    return nullptr;
  }
  // out of range means the java enum lags the C++ one; every JNI call after a
  // pending ArrayIndexOutOfBoundsException would be undefined
  jobject result = code < env->GetArrayLength(array)
                       ? env->GetObjectArrayElement(array, code)
                       : nullptr;
  env->DeleteLocalRef(array);
  return result;
}

jint enum_ordinal(JNIEnv *env, jobject value) {
  if (value == nullptr) {
    return -1;
  }
  jclass cls = env->FindClass("java/lang/Enum");
  jmethodID ordinal = env->GetMethodID(cls, "ordinal", "()I");
  const jint result = env->CallIntMethod(value, ordinal);
  env->DeleteLocalRef(cls);
  return result;
}

} // namespace

jstring make_string_opt(JNIEnv *env, const std::optional<std::string> &value) {
  return value.has_value() ? to_jstring(env, *value) : nullptr;
}

jstring make_string_opt(JNIEnv *env,
                        const std::optional<std::string_view> &value) {
  return value.has_value() ? to_jstring(env, *value) : nullptr;
}

jobject make_integer_opt(JNIEnv *env,
                         const std::optional<std::int32_t> &value) {
  if (!value.has_value()) {
    return nullptr;
  }
  return call_static_object(env, "java/lang/Integer", "valueOf",
                            "(I)Ljava/lang/Integer;",
                            static_cast<jint>(*value));
}

jobject make_measure(JNIEnv *env, const odr::Measure &value) {
  return new_object(env, "app/opendocument/core/Measure",
                    "(DLjava/lang/String;)V", value.magnitude(),
                    to_jstring(env, value.unit().to_string()));
}

jobject make_measure(JNIEnv *env, const std::optional<odr::Measure> &value) {
  return value.has_value() ? make_measure(env, *value) : nullptr;
}

jobject make_color(JNIEnv *env, const std::optional<odr::Color> &value) {
  if (!value.has_value()) {
    return nullptr;
  }
  return new_object(
      env, "app/opendocument/core/Color", "(IIII)V",
      static_cast<jint>(value->red), static_cast<jint>(value->green),
      static_cast<jint>(value->blue), static_cast<jint>(value->alpha));
}

jobject
make_directional_measure(JNIEnv *env,
                         const odr::DirectionalStyle<odr::Measure> &style) {
  return new_object(
      env, "app/opendocument/core/DirectionalMeasure",
      "(Lapp/opendocument/core/Measure;Lapp/opendocument/core/Measure;"
      "Lapp/opendocument/core/Measure;Lapp/opendocument/core/Measure;)V",
      make_measure(env, style.right), make_measure(env, style.top),
      make_measure(env, style.left), make_measure(env, style.bottom));
}

jobject
make_directional_string(JNIEnv *env,
                        const odr::DirectionalStyle<std::string> &style) {
  return new_object(
      env, "app/opendocument/core/DirectionalString",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
      "Ljava/lang/String;)V",
      make_string_opt(env, style.right), make_string_opt(env, style.top),
      make_string_opt(env, style.left), make_string_opt(env, style.bottom));
}

jobject make_text_style(JNIEnv *env, const odr::TextStyle &style) {
  return new_object(
      env, "app/opendocument/core/TextStyle",
      "(Ljava/lang/String;Lapp/opendocument/core/Measure;II"
      "Ljava/lang/Boolean;Ljava/lang/Boolean;Ljava/lang/String;"
      "Lapp/opendocument/core/Color;Lapp/opendocument/core/Color;I)V",
      make_string_opt(env, style.font_name), make_measure(env, style.font_size),
      enum_code(style.font_weight), enum_code(style.font_style),
      box_boolean(env, style.font_underline),
      box_boolean(env, style.font_line_through),
      make_string_opt(env, style.font_shadow),
      make_color(env, style.font_color),
      make_color(env, style.background_color), enum_code(style.font_position));
}

jobject make_paragraph_style(JNIEnv *env, const odr::ParagraphStyle &style) {
  return new_object(
      env, "app/opendocument/core/ParagraphStyle",
      "(ILapp/opendocument/core/DirectionalMeasure;"
      "Lapp/opendocument/core/Measure;Lapp/opendocument/core/Measure;)V",
      enum_code(style.text_align), make_directional_measure(env, style.margin),
      make_measure(env, style.line_height),
      make_measure(env, style.text_indent));
}

jobject make_table_style(JNIEnv *env, const odr::TableStyle &style) {
  return new_object(env, "app/opendocument/core/TableStyle",
                    "(Lapp/opendocument/core/Measure;)V",
                    make_measure(env, style.width));
}

jobject make_table_column_style(JNIEnv *env,
                                const odr::TableColumnStyle &style) {
  return new_object(env, "app/opendocument/core/TableColumnStyle",
                    "(Lapp/opendocument/core/Measure;)V",
                    make_measure(env, style.width));
}

jobject make_table_row_style(JNIEnv *env, const odr::TableRowStyle &style) {
  return new_object(env, "app/opendocument/core/TableRowStyle",
                    "(Lapp/opendocument/core/Measure;)V",
                    make_measure(env, style.height));
}

jobject make_table_cell_style(JNIEnv *env, const odr::TableCellStyle &style) {
  return new_object(
      env, "app/opendocument/core/TableCellStyle",
      "(IILapp/opendocument/core/Color;"
      "Lapp/opendocument/core/DirectionalMeasure;"
      "Lapp/opendocument/core/DirectionalString;Ljava/lang/Double;)V",
      enum_code(style.horizontal_align), enum_code(style.vertical_align),
      make_color(env, style.background_color),
      make_directional_measure(env, style.padding),
      make_directional_string(env, style.border),
      box_double(env, style.text_rotation));
}

jobject make_graphic_style(JNIEnv *env, const odr::GraphicStyle &style) {
  return new_object(
      env, "app/opendocument/core/GraphicStyle",
      "(Lapp/opendocument/core/Measure;Lapp/opendocument/core/Color;"
      "Lapp/opendocument/core/Color;III)V",
      make_measure(env, style.stroke_width),
      make_color(env, style.stroke_color), make_color(env, style.fill_color),
      enum_code(style.vertical_align), enum_code(style.text_wrap),
      enum_code(style.horizontal_position));
}

jobject make_page_layout(JNIEnv *env, const odr::PageLayout &layout) {
  return new_object(
      env, "app/opendocument/core/PageLayout",
      "(Lapp/opendocument/core/Measure;Lapp/opendocument/core/Measure;I"
      "Lapp/opendocument/core/DirectionalMeasure;)V",
      make_measure(env, layout.width), make_measure(env, layout.height),
      enum_code(layout.print_orientation),
      make_directional_measure(env, layout.margin));
}

jobject make_table_dimensions(JNIEnv *env,
                              const odr::TableDimensions &dimensions) {
  return new_object(env, "app/opendocument/core/TableDimensions", "(II)V",
                    static_cast<jint>(dimensions.rows),
                    static_cast<jint>(dimensions.columns));
}

jobject make_table_position(JNIEnv *env, const odr::TablePosition &position) {
  return new_object(env, "app/opendocument/core/TablePosition", "(II)V",
                    static_cast<jint>(position.column),
                    static_cast<jint>(position.row));
}

jobject make_file_meta(JNIEnv *env, const odr::FileMeta &meta) {
  return new_object(
      env, "app/opendocument/core/FileMeta",
      "(ILjava/lang/String;ZILjava/lang/Long;Ljava/lang/String;"
      "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
      "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
      "Ljava/lang/String;)V",
      static_cast<jint>(meta.type), to_jstring(env, meta.mimetype),
      static_cast<jboolean>(meta.password_encrypted),
      static_cast<jint>(meta.document_type), box_long(env, meta.entry_count),
      make_string_opt(env, meta.title), make_string_opt(env, meta.author),
      make_string_opt(env, meta.subject), make_string_opt(env, meta.keywords),
      make_string_opt(env, meta.creator), make_string_opt(env, meta.producer),
      make_string_opt(env, meta.creation_date),
      make_string_opt(env, meta.modification_date));
}

jobject
make_file_type_capabilities(JNIEnv *env,
                            const odr::FileTypeCapabilities &capabilities) {
  return new_object(env, "app/opendocument/core/FileTypeCapabilities",
                    "(ZZZZZZZ)V",
                    static_cast<jboolean>(capabilities.detect_by_content),
                    static_cast<jboolean>(capabilities.open),
                    static_cast<jboolean>(capabilities.decrypt),
                    static_cast<jboolean>(capabilities.translate_html),
                    static_cast<jboolean>(capabilities.edit),
                    static_cast<jboolean>(capabilities.save),
                    static_cast<jboolean>(capabilities.encrypt));
}

jobject html_config_to_java(JNIEnv *env, const odr::HtmlConfig &config) {
  jclass cls = env->FindClass("app/opendocument/core/HtmlConfig");
  if (cls == nullptr) {
    return nullptr;
  }
  jmethodID ctor = env->GetMethodID(cls, "<init>", "()V");
  jobject result = env->NewObject(cls, ctor);

  const auto set_string = [&](const char *name, const std::string &value) {
    env->SetObjectField(result,
                        env->GetFieldID(cls, name, "Ljava/lang/String;"),
                        to_jstring(env, value));
  };
  const auto set_boolean = [&](const char *name, const bool value) {
    env->SetBooleanField(result, env->GetFieldID(cls, name, "Z"),
                         static_cast<jboolean>(value));
  };
  const auto set_int = [&](const char *name, const jint value) {
    env->SetIntField(result, env->GetFieldID(cls, name, "I"), value);
  };
  const auto set_double = [&](const char *name, const double value) {
    env->SetDoubleField(result, env->GetFieldID(cls, name, "D"), value);
  };
  const auto set_object = [&](const char *name, const char *signature,
                              jobject value) {
    env->SetObjectField(result, env->GetFieldID(cls, name, signature), value);
  };

  set_string("documentOutputFileName", config.document_output_file_name);
  set_string("slideOutputFileName", config.slide_output_file_name);
  set_string("sheetOutputFileName", config.sheet_output_file_name);
  set_string("pageOutputFileName", config.page_output_file_name);
  set_boolean("embedImages", config.embed_images);
  set_boolean("embedShippedResources", config.embed_shipped_resources);
  set_string("resourcePath", config.resource_path);
  set_boolean("relativeResourcePaths", config.relative_resource_paths);
  set_boolean("editable", config.editable);
  set_boolean("textDocumentMargin", config.text_document_margin);
  set_object("spreadsheetLimit", "Lapp/opendocument/core/TableDimensions;",
             config.spreadsheet_limit.has_value()
                 ? make_table_dimensions(env, *config.spreadsheet_limit)
                 : nullptr);
  set_boolean("spreadsheetLimitByContent", config.spreadsheet_limit_by_content);
  set_object("spreadsheetGridlines",
             "Lapp/opendocument/core/HtmlTableGridlines;",
             enum_from_code(env, "app/opendocument/core/HtmlTableGridlines",
                            static_cast<jint>(config.spreadsheet_gridlines)));
  set_object("viewportMode", "Lapp/opendocument/core/HtmlViewportMode;",
             enum_from_code(env, "app/opendocument/core/HtmlViewportMode",
                            static_cast<jint>(config.viewport_mode)));
  set_object(
      "spreadsheetViewportMode", "Lapp/opendocument/core/HtmlViewportMode;",
      enum_from_code(env, "app/opendocument/core/HtmlViewportMode",
                     config.spreadsheet_viewport_mode.has_value()
                         ? static_cast<jint>(*config.spreadsheet_viewport_mode)
                         : -1));
  set_object("viewportContent", "Ljava/lang/String;",
             make_string_opt(env, config.viewport_content));
  set_boolean("formatHtml", config.format_html);
  set_int("htmlIndent", config.html_indent);
  set_string("htmlIndentString", config.html_indent_string);
  set_string("backgroundImageFormat", config.background_image_format);
  set_double("backgroundImageDpi", config.background_image_dpi);
  set_int("pageRangeBegin", static_cast<jint>(config.page_range_begin));
  set_object("pageRangeEnd", "Ljava/lang/Integer;",
             box_integer(env, config.page_range_end));
  set_object("pdfTextMode", "Lapp/opendocument/core/PdfTextMode;",
             enum_from_code(env, "app/opendocument/core/PdfTextMode",
                            static_cast<jint>(config.pdf_text_mode)));
  {
    jclass string_cls = env->FindClass("java/lang/String");
    jobjectArray fonts = env->NewObjectArray(
        static_cast<jsize>(config.pdf_dual_layer_fallback_fonts.size()),
        string_cls, nullptr);
    for (jsize i = 0;
         i < static_cast<jsize>(config.pdf_dual_layer_fallback_fonts.size());
         ++i) {
      jstring font = to_jstring(env, config.pdf_dual_layer_fallback_fonts[i]);
      env->SetObjectArrayElement(fonts, i, font);
      env->DeleteLocalRef(font);
    }
    set_object("pdfDualLayerFallbackFonts", "[Ljava/lang/String;", fonts);
    env->DeleteLocalRef(string_cls);
  }
  set_double("pdfDualLayerFallbackFontSizeAdjust",
             config.pdf_dual_layer_fallback_font_size_adjust);
  set_boolean("noDrm", config.no_drm);
  set_boolean("embedOutline", config.embed_outline);
  set_object("outputPath", "Ljava/lang/String;",
             make_string_opt(env, config.output_path));

  env->DeleteLocalRef(cls);
  return result;
}

odr::HtmlConfig html_config_from_java(JNIEnv *env, jobject config) {
  // Start from the C++ defaults so unexposed members (resource_locator) keep
  // their standard values.
  odr::HtmlConfig result;
  if (config == nullptr) {
    return result;
  }

  jclass cls = env->GetObjectClass(config);

  const auto get_string = [&](const char *name) {
    auto value = static_cast<jstring>(env->GetObjectField(
        config, env->GetFieldID(cls, name, "Ljava/lang/String;")));
    return to_string(env, value);
  };
  const auto get_string_opt =
      [&](const char *name) -> std::optional<std::string> {
    auto value = static_cast<jstring>(env->GetObjectField(
        config, env->GetFieldID(cls, name, "Ljava/lang/String;")));
    if (value == nullptr) {
      return std::nullopt;
    }
    return to_string(env, value);
  };
  const auto get_boolean = [&](const char *name) {
    return env->GetBooleanField(config, env->GetFieldID(cls, name, "Z")) !=
           JNI_FALSE;
  };
  const auto get_int = [&](const char *name) {
    return env->GetIntField(config, env->GetFieldID(cls, name, "I"));
  };
  const auto get_double = [&](const char *name) {
    return env->GetDoubleField(config, env->GetFieldID(cls, name, "D"));
  };
  const auto get_object = [&](const char *name, const char *signature) {
    return env->GetObjectField(config, env->GetFieldID(cls, name, signature));
  };

  result.document_output_file_name = get_string("documentOutputFileName");
  result.slide_output_file_name = get_string("slideOutputFileName");
  result.sheet_output_file_name = get_string("sheetOutputFileName");
  result.page_output_file_name = get_string("pageOutputFileName");
  result.embed_images = get_boolean("embedImages");
  result.embed_shipped_resources = get_boolean("embedShippedResources");
  if (const auto resource_path = get_string_opt("resourcePath");
      resource_path.has_value()) {
    result.resource_path = *resource_path;
  }
  result.relative_resource_paths = get_boolean("relativeResourcePaths");
  result.editable = get_boolean("editable");
  result.text_document_margin = get_boolean("textDocumentMargin");
  {
    jobject limit = get_object("spreadsheetLimit",
                               "Lapp/opendocument/core/TableDimensions;");
    if (limit == nullptr) {
      result.spreadsheet_limit = std::nullopt;
    } else {
      jclass dimensions_cls = env->GetObjectClass(limit);
      const jint rows =
          env->GetIntField(limit, env->GetFieldID(dimensions_cls, "rows", "I"));
      const jint columns = env->GetIntField(
          limit, env->GetFieldID(dimensions_cls, "columns", "I"));
      result.spreadsheet_limit =
          odr::TableDimensions(static_cast<std::uint32_t>(rows),
                               static_cast<std::uint32_t>(columns));
      env->DeleteLocalRef(dimensions_cls);
    }
  }
  result.spreadsheet_limit_by_content =
      get_boolean("spreadsheetLimitByContent");
  {
    const jint code = enum_ordinal(
        env, get_object("spreadsheetGridlines",
                        "Lapp/opendocument/core/HtmlTableGridlines;"));
    if (code >= 0) {
      result.spreadsheet_gridlines = static_cast<odr::HtmlTableGridlines>(code);
    }
  }
  {
    const jint code = enum_ordinal(
        env,
        get_object("viewportMode", "Lapp/opendocument/core/HtmlViewportMode;"));
    if (code >= 0) {
      result.viewport_mode = static_cast<odr::HtmlViewportMode>(code);
    }
  }
  {
    const jint code = enum_ordinal(
        env, get_object("spreadsheetViewportMode",
                        "Lapp/opendocument/core/HtmlViewportMode;"));
    result.spreadsheet_viewport_mode =
        code < 0 ? std::nullopt
                 : std::make_optional(static_cast<odr::HtmlViewportMode>(code));
  }
  result.viewport_content = get_string_opt("viewportContent");
  result.format_html = get_boolean("formatHtml");
  result.html_indent = static_cast<std::uint8_t>(get_int("htmlIndent"));
  result.html_indent_string = get_string("htmlIndentString");
  result.background_image_format = get_string("backgroundImageFormat");
  result.background_image_dpi = get_double("backgroundImageDpi");
  result.page_range_begin =
      static_cast<std::uint32_t>(get_int("pageRangeBegin"));
  {
    jobject end = get_object("pageRangeEnd", "Ljava/lang/Integer;");
    if (end == nullptr) {
      result.page_range_end = std::nullopt;
    } else {
      jclass integer_cls = env->GetObjectClass(end);
      jmethodID int_value = env->GetMethodID(integer_cls, "intValue", "()I");
      result.page_range_end =
          static_cast<std::uint32_t>(env->CallIntMethod(end, int_value));
      env->DeleteLocalRef(integer_cls);
    }
  }
  {
    const jint code = enum_ordinal(
        env, get_object("pdfTextMode", "Lapp/opendocument/core/PdfTextMode;"));
    if (code >= 0) {
      result.pdf_text_mode = static_cast<odr::PdfTextMode>(code);
    }
  }
  {
    auto fonts = static_cast<jobjectArray>(
        get_object("pdfDualLayerFallbackFonts", "[Ljava/lang/String;"));
    if (fonts != nullptr) {
      const jsize length = env->GetArrayLength(fonts);
      result.pdf_dual_layer_fallback_fonts.clear();
      for (jsize i = 0; i < length; ++i) {
        auto font = static_cast<jstring>(env->GetObjectArrayElement(fonts, i));
        result.pdf_dual_layer_fallback_fonts.push_back(to_string(env, font));
        env->DeleteLocalRef(font);
      }
    }
  }
  result.pdf_dual_layer_fallback_font_size_adjust =
      get_double("pdfDualLayerFallbackFontSizeAdjust");
  result.no_drm = get_boolean("noDrm");
  result.embed_outline = get_boolean("embedOutline");
  result.output_path = get_string_opt("outputPath");

  env->DeleteLocalRef(cls);
  return result;
}

} // namespace odr_jni
