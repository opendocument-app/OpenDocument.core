#include "jni_convert.hpp"
#include "odr_jni.hpp"

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>

#include <sstream>
#include <vector>

namespace {

using odr_jni::destroy_handle;
using odr_jni::from_handle;
using odr_jni::guarded;
using odr_jni::HandleGuard;
using odr_jni::make_handle;
using odr_jni::to_jbytes;
using odr_jni::to_jstring;
using odr_jni::to_string;

odr::Element &element(jlong handle) {
  return *from_handle<odr::Element>(handle);
}

/// Elements cross JNI as plain `odr::Element` copies; typed Java views
/// re-derive the typed C++ view per call via `as_*`.
jlong wrap_element(odr::Element value) {
  if (!value) {
    return 0;
  }
  return make_handle(std::move(value));
}

jlongArray wrap_elements(JNIEnv *env, const odr::ElementRange &range) {
  HandleGuard<odr::Element> guard;
  for (const odr::Element &value : range) {
    guard.add(value);
  }
  const std::vector<jlong> &handles = guard.handles();
  jlongArray result = env->NewLongArray(static_cast<jsize>(handles.size()));
  if (result == nullptr) {
    return nullptr;
  }
  env->SetLongArrayRegion(result, 0, static_cast<jsize>(handles.size()),
                          handles.data());
  guard.release();
  return result;
}

} // namespace

// app.opendocument.core.Document

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Document_destroy(JNIEnv *env, jclass, jlong handle) {
  destroy_handle<odr::Document>(env, handle);
}

// odr::html::edit, but it belongs to Document: a native taking a handle must be
// an instance method of its owner, or the wrapper can be collected mid-call.
extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Document_editNative(JNIEnv *env, jobject,
                                               jlong handle, jstring diff) {
  guarded(env, [&] {
    odr::html::edit(*from_handle<odr::Document>(handle), to_string(env, diff));
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Document_isEditableNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::Document>(handle)->is_editable());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Document_isSavableNative(JNIEnv *env, jobject,
                                                    jlong handle,
                                                    jboolean encrypted) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::Document>(handle)->is_savable(encrypted != JNI_FALSE));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Document_saveNative(JNIEnv *env, jobject,
                                               jlong handle, jstring path) {
  guarded(env, [&] {
    from_handle<odr::Document>(handle)->save(to_string(env, path));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Document_saveEncryptedNative(JNIEnv *env, jobject,
                                                        jlong handle,
                                                        jstring path,
                                                        jstring password) {
  guarded(env, [&] {
    from_handle<odr::Document>(handle)->save(to_string(env, path),
                                             to_string(env, password));
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_Document_saveToMemoryNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    std::ostringstream out;
    from_handle<odr::Document>(handle)->save(out);
    return to_jbytes(env, out.str());
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_Document_saveToMemoryEncryptedNative(
    JNIEnv *env, jobject, jlong handle, jstring password) {
  return guarded(env, [&] {
    std::ostringstream out;
    from_handle<odr::Document>(handle)->save(out, to_string(env, password));
    return to_jbytes(env, out.str());
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Document_fileTypeNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(from_handle<odr::Document>(handle)->file_type());
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Document_documentTypeNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(
        from_handle<odr::Document>(handle)->document_type());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Document_rootElementNative(JNIEnv *env, jobject,
                                                      jlong handle) {
  return guarded(env, [&] {
    return wrap_element(from_handle<odr::Document>(handle)->root_element());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Document_asFilesystemNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    return make_handle(from_handle<odr::Document>(handle)->as_filesystem());
  });
}

// app.opendocument.core.DocumentPath

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DocumentPath_create(JNIEnv *env, jclass,
                                               jstring path) {
  return guarded(env, [&] {
    return make_handle(odr::DocumentPath(to_string(env, path)));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_DocumentPath_destroy(JNIEnv *env, jclass,
                                                jlong handle) {
  destroy_handle<odr::DocumentPath>(env, handle);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DocumentPath_emptyNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::DocumentPath>(handle)->empty());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DocumentPath_parentNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return make_handle(from_handle<odr::DocumentPath>(handle)->parent());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DocumentPath_joinNative(JNIEnv *env, jobject,
                                                   jlong handle,
                                                   jlong other_handle) {
  return guarded(env, [&] {
    return make_handle(from_handle<odr::DocumentPath>(handle)->join(
        *from_handle<odr::DocumentPath>(other_handle)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_DocumentPath_toStringNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    return to_jstring(env, from_handle<odr::DocumentPath>(handle)->to_string());
  });
}

// app.opendocument.core.Element

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Element_destroy(JNIEnv *env, jclass, jlong handle) {
  destroy_handle<odr::Element>(env, handle);
}

extern "C" JNIEXPORT jint JNICALL Java_app_opendocument_core_Element_typeNative(
    JNIEnv *env, jobject, jlong handle) {
  return guarded(env,
                 [&] { return static_cast<jint>(element(handle).type()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Element_parentNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] { return wrap_element(element(handle).parent()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Element_firstChildNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  return guarded(env,
                 [&] { return wrap_element(element(handle).first_child()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Element_previousSiblingNative(JNIEnv *env, jobject,
                                                         jlong handle) {
  return guarded(
      env, [&] { return wrap_element(element(handle).previous_sibling()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Element_nextSiblingNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env,
                 [&] { return wrap_element(element(handle).next_sibling()); });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Element_isUniqueNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(
      env, [&] { return static_cast<jboolean>(element(handle).is_unique()); });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Element_isSelfLocatableNative(JNIEnv *env, jobject,
                                                         jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(element(handle).is_self_locatable());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Element_isEditableNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(element(handle).is_editable());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Element_isSameNative(JNIEnv *env, jobject,
                                                jlong handle,
                                                jlong other_handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(element(handle) == element(other_handle));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Element_documentPathNative(JNIEnv *env, jobject,
                                                      jlong handle) {
  return guarded(env,
                 [&] { return make_handle(element(handle).document_path()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Element_navigatePathNative(JNIEnv *env, jobject,
                                                      jlong handle,
                                                      jlong path_handle) {
  return guarded(env, [&] {
    return wrap_element(element(handle).navigate_path(
        *from_handle<odr::DocumentPath>(path_handle)));
  });
}

// The typed accessors return a fresh `odr::Element` copy when the element
// supports the typed view, 0 otherwise.
#define ODR_JNI_ELEMENT_AS(java_name, method)                                  \
  extern "C" JNIEXPORT jlong JNICALL                                           \
      Java_app_opendocument_core_Element_as##java_name##Native(                \
          JNIEnv *env, jobject, jlong handle) {                                \
    return guarded(env, [&]() -> jlong {                                       \
      if (!element(handle).method()) {                                         \
        return 0;                                                              \
      }                                                                        \
      return make_handle(odr::Element(element(handle)));                       \
    });                                                                        \
  }

ODR_JNI_ELEMENT_AS(TextRoot, as_text_root)
ODR_JNI_ELEMENT_AS(Slide, as_slide)
ODR_JNI_ELEMENT_AS(Sheet, as_sheet)
ODR_JNI_ELEMENT_AS(Page, as_page)
ODR_JNI_ELEMENT_AS(SheetCell, as_sheet_cell)
ODR_JNI_ELEMENT_AS(MasterPage, as_master_page)
ODR_JNI_ELEMENT_AS(LineBreak, as_line_break)
ODR_JNI_ELEMENT_AS(Paragraph, as_paragraph)
ODR_JNI_ELEMENT_AS(Span, as_span)
ODR_JNI_ELEMENT_AS(Text, as_text)
ODR_JNI_ELEMENT_AS(Link, as_link)
ODR_JNI_ELEMENT_AS(Bookmark, as_bookmark)
ODR_JNI_ELEMENT_AS(List, as_list)
ODR_JNI_ELEMENT_AS(ListItem, as_list_item)
ODR_JNI_ELEMENT_AS(Table, as_table)
ODR_JNI_ELEMENT_AS(TableColumn, as_table_column)
ODR_JNI_ELEMENT_AS(TableRow, as_table_row)
ODR_JNI_ELEMENT_AS(TableCell, as_table_cell)
ODR_JNI_ELEMENT_AS(Frame, as_frame)
ODR_JNI_ELEMENT_AS(Rect, as_rect)
ODR_JNI_ELEMENT_AS(Line, as_line)
ODR_JNI_ELEMENT_AS(Circle, as_circle)
ODR_JNI_ELEMENT_AS(CustomShape, as_custom_shape)
ODR_JNI_ELEMENT_AS(Image, as_image)

#undef ODR_JNI_ELEMENT_AS

// app.opendocument.core.TextRoot

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_TextRoot_pageLayoutNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_page_layout(
        env, element(handle).as_text_root().page_layout());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_TextRoot_firstMasterPageNative(JNIEnv *env, jobject,
                                                          jlong handle) {
  return guarded(env, [&] {
    return wrap_element(element(handle).as_text_root().first_master_page());
  });
}

// app.opendocument.core.Slide

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Slide_nameNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(
      env, [&] { return to_jstring(env, element(handle).as_slide().name()); });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Slide_pageLayoutNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_page_layout(env,
                                     element(handle).as_slide().page_layout());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Slide_masterPageNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    return wrap_element(element(handle).as_slide().master_page());
  });
}

// app.opendocument.core.Sheet

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Sheet_nameNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(
      env, [&] { return to_jstring(env, element(handle).as_sheet().name()); });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Sheet_dimensionsNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_dimensions(
        env, element(handle).as_sheet().dimensions());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Sheet_contentNative(JNIEnv *env, jobject,
                                               jlong handle, jint rows,
                                               jint columns) {
  return guarded(env, [&] {
    std::optional<odr::TableDimensions> range;
    if (rows >= 0 && columns >= 0) {
      range = odr::TableDimensions(static_cast<std::uint32_t>(rows),
                                   static_cast<std::uint32_t>(columns));
    }
    return odr_jni::make_table_dimensions(
        env, element(handle).as_sheet().content(range));
  });
}

extern "C" JNIEXPORT jlong JNICALL Java_app_opendocument_core_Sheet_cellNative(
    JNIEnv *env, jobject, jlong handle, jint column, jint row) {
  return guarded(env, [&] {
    return wrap_element(element(handle).as_sheet().cell(
        static_cast<std::uint32_t>(column), static_cast<std::uint32_t>(row)));
  });
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_app_opendocument_core_Sheet_shapesNative(JNIEnv *env, jobject,
                                              jlong handle) {
  return guarded(env, [&] {
    return wrap_elements(env, element(handle).as_sheet().shapes());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Sheet_styleNative(JNIEnv *env, jobject,
                                             jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_style(env, element(handle).as_sheet().style());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Sheet_columnStyleNative(JNIEnv *env, jobject,
                                                   jlong handle, jint column) {
  return guarded(env, [&] {
    return odr_jni::make_table_column_style(
        env, element(handle).as_sheet().column_style(
                 static_cast<std::uint32_t>(column)));
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Sheet_rowStyleNative(JNIEnv *env, jobject,
                                                jlong handle, jint row) {
  return guarded(env, [&] {
    return odr_jni::make_table_row_style(
        env,
        element(handle).as_sheet().row_style(static_cast<std::uint32_t>(row)));
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Sheet_cellStyleNative(JNIEnv *env, jobject,
                                                 jlong handle, jint column,
                                                 jint row) {
  return guarded(env, [&] {
    return odr_jni::make_table_cell_style(
        env, element(handle).as_sheet().cell_style(
                 static_cast<std::uint32_t>(column),
                 static_cast<std::uint32_t>(row)));
  });
}

// app.opendocument.core.SheetCell

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_SheetCell_positionNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_position(
        env, element(handle).as_sheet_cell().position());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_SheetCell_isCoveredNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(element(handle).as_sheet_cell().is_covered());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_SheetCell_spanNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_dimensions(
        env, element(handle).as_sheet_cell().span());
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_SheetCell_valueTypeNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(element(handle).as_sheet_cell().value_type());
  });
}

// app.opendocument.core.Page

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Page_nameNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(
      env, [&] { return to_jstring(env, element(handle).as_page().name()); });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Page_pageLayoutNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_page_layout(env,
                                     element(handle).as_page().page_layout());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Page_masterPageNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return wrap_element(element(handle).as_page().master_page());
  });
}

// app.opendocument.core.MasterPage

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_MasterPage_pageLayoutNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_page_layout(
        env, element(handle).as_master_page().page_layout());
  });
}

// app.opendocument.core.LineBreak

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_LineBreak_styleNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_text_style(env,
                                    element(handle).as_line_break().style());
  });
}

// app.opendocument.core.Paragraph

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Paragraph_styleNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_paragraph_style(
        env, element(handle).as_paragraph().style());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Paragraph_textStyleNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_text_style(
        env, element(handle).as_paragraph().text_style());
  });
}

// app.opendocument.core.Span

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Span_styleNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_text_style(env, element(handle).as_span().style());
  });
}

// app.opendocument.core.Text

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Text_contentNative(JNIEnv *env, jobject,
                                              jlong handle) {
  return guarded(env, [&] {
    return to_jstring(env, element(handle).as_text().content());
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Text_setContentNative(JNIEnv *env, jobject,
                                                 jlong handle, jstring text) {
  guarded(env,
          [&] { element(handle).as_text().set_content(to_string(env, text)); });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Text_styleNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_text_style(env, element(handle).as_text().style());
  });
}

// app.opendocument.core.Link

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Link_hrefNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(
      env, [&] { return to_jstring(env, element(handle).as_link().href()); });
}

// app.opendocument.core.Bookmark

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Bookmark_nameNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return to_jstring(env, element(handle).as_bookmark().name());
  });
}

// app.opendocument.core.ListElement

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_ListElement_listTypeNative(JNIEnv *env, jobject,
                                                      jlong handle) {
  return guarded(
      env, [&] { return static_cast<jint>(element(handle).as_list().type()); });
}

// app.opendocument.core.ListItem

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_ListItem_styleNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_text_style(env,
                                    element(handle).as_list_item().style());
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_ListItem_markerNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return to_jstring(env, element(handle).as_list_item().marker());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_ListItem_numberNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    std::optional<std::int32_t> number;
    if (const std::optional<std::uint32_t> value =
            element(handle).as_list_item().number();
        value.has_value()) {
      number = static_cast<std::int32_t>(*value);
    }
    return odr_jni::make_integer_opt(env, number);
  });
}

// app.opendocument.core.Table

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Table_firstRowNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] {
    return wrap_element(element(handle).as_table().first_row());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Table_firstColumnNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] {
    return wrap_element(element(handle).as_table().first_column());
  });
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_app_opendocument_core_Table_columnsNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return wrap_elements(env, element(handle).as_table().columns());
  });
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_app_opendocument_core_Table_rowsNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(env, [&] {
    return wrap_elements(env, element(handle).as_table().rows());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Table_dimensionsNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_dimensions(
        env, element(handle).as_table().dimensions());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Table_styleNative(JNIEnv *env, jobject,
                                             jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_style(env, element(handle).as_table().style());
  });
}

// app.opendocument.core.TableColumn

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_TableColumn_styleNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_column_style(
        env, element(handle).as_table_column().style());
  });
}

// app.opendocument.core.TableRow

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_TableRow_styleNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_row_style(
        env, element(handle).as_table_row().style());
  });
}

// app.opendocument.core.TableCell

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_TableCell_isCoveredNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(element(handle).as_table_cell().is_covered());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_TableCell_spanNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_dimensions(
        env, element(handle).as_table_cell().span());
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_TableCell_valueTypeNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(element(handle).as_table_cell().value_type());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_TableCell_styleNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_table_cell_style(
        env, element(handle).as_table_cell().style());
  });
}

// app.opendocument.core.Frame

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Frame_anchorTypeNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(element(handle).as_frame().anchor_type());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Frame_xNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_frame().x());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Frame_yNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_frame().y());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Frame_widthNative(JNIEnv *env, jobject,
                                             jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_frame().width());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Frame_heightNative(JNIEnv *env, jobject,
                                              jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_frame().height());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Frame_zIndexNative(JNIEnv *env, jobject,
                                              jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_integer_opt(env, element(handle).as_frame().z_index());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Frame_styleNative(JNIEnv *env, jobject,
                                             jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_graphic_style(env, element(handle).as_frame().style());
  });
}

// app.opendocument.core.Rect

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Rect_xNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_rect().x());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Rect_yNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_rect().y());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Rect_widthNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_rect().width());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Rect_heightNative(JNIEnv *env, jobject,
                                             jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_rect().height());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Rect_styleNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_graphic_style(env, element(handle).as_rect().style());
  });
}

// app.opendocument.core.Line

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Line_x1Native(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_line().x1());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Line_y1Native(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_line().y1());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Line_x2Native(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_line().x2());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Line_y2Native(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_line().y2());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Line_styleNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_graphic_style(env, element(handle).as_line().style());
  });
}

// app.opendocument.core.Circle

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Circle_xNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_circle().x());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Circle_yNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_circle().y());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Circle_widthNative(JNIEnv *env, jobject,
                                              jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_circle().width());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Circle_heightNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_circle().height());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Circle_styleNative(JNIEnv *env, jobject,
                                              jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_graphic_style(env,
                                       element(handle).as_circle().style());
  });
}

// app.opendocument.core.CustomShape

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_CustomShape_xNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_custom_shape().x());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_CustomShape_yNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env, element(handle).as_custom_shape().y());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_CustomShape_widthNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env,
                                 element(handle).as_custom_shape().width());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_CustomShape_heightNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_measure(env,
                                 element(handle).as_custom_shape().height());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_CustomShape_styleNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_graphic_style(
        env, element(handle).as_custom_shape().style());
  });
}

// app.opendocument.core.Image

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Image_isInternalNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(element(handle).as_image().is_internal());
  });
}

extern "C" JNIEXPORT jlong JNICALL Java_app_opendocument_core_Image_fileNative(
    JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&]() -> jlong {
    const std::optional<odr::File> file = element(handle).as_image().file();
    if (!file.has_value()) {
      return 0;
    }
    return make_handle(odr::File(*file));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Image_hrefNative(JNIEnv *env, jobject,
                                            jlong handle) {
  return guarded(
      env, [&] { return to_jstring(env, element(handle).as_image().href()); });
}
