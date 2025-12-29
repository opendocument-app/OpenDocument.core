#pragma once

#include <odr/definitions.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>

namespace pugi {
class xml_node;
}

namespace odr::internal::ooxml::spreadsheet {
class ElementRegistry;

class ParseContext {
public:
  ParseContext(const AbsPath &document_path,
               const Relations &document_relations,
               const XmlDocumentsAndRelations &xml_documents_and_relations,
               const SharedStrings &shared_strings)
      : m_document_path(&document_path),
        m_document_relations(&document_relations),
        m_xml_documents_and_relations(&xml_documents_and_relations),
        m_shared_strings(&shared_strings) {}

  [[nodiscard]] const AbsPath &document_path() const {
    return *m_document_path;
  }
  [[nodiscard]] const Relations &document_relations() const {
    return *m_document_relations;
  }
  [[nodiscard]] const XmlDocumentsAndRelations &
  documents_and_relations() const {
    return *m_xml_documents_and_relations;
  }
  [[nodiscard]] const SharedStrings &shared_strings() const {
    return *m_shared_strings;
  }

private:
  const AbsPath *m_document_path{nullptr};
  const Relations *m_document_relations{nullptr};
  const XmlDocumentsAndRelations *m_xml_documents_and_relations{nullptr};
  const SharedStrings *m_shared_strings{nullptr};
};

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const ParseContext &context, pugi::xml_node node);

} // namespace odr::internal::ooxml::spreadsheet
