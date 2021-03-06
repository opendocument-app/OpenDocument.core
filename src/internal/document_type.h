#ifndef ODR_DOCUMENT_TYPE_H
#define ODR_DOCUMENT_TYPE_H

namespace odr::internal {

enum class DocumentType {
  UNKNOWN,
  TEXT,
  PRESENTATION,
  SPREADSHEET,
  DRAWING,
};

}

#endif // ODR_DOCUMENT_TYPE_H
