#ifndef ODR_INTERNAL_HTML_DOCUMENT_H
#define ODR_INTERNAL_HTML_DOCUMENT_H

#include <iosfwd>

namespace odr {
class Document;
class DocumentCursor;

struct HtmlConfig;
} // namespace odr

namespace odr::internal::html {

void translate_document(const Document &document, std::ostream &out,
                        const HtmlConfig &config);

void translate_text_document(DocumentCursor &cursor, std::ostream &out,
                             const HtmlConfig &config);
void translate_presentation(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config);
void translate_spreadsheet(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config);
void translate_drawing(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config);

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_DOCUMENT_H
