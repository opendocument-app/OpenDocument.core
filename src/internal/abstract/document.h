#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_H

#include <any>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace odr {
enum class DocumentType;
struct DocumentMeta;
enum class ElementType;
enum class ElementProperty;
struct TableDimensions;
} // namespace odr

namespace odr::internal::common {
class Path;
}

namespace odr::internal::abstract {
class File;

template <typename Number, typename Tag> struct Identifier {
  Identifier() = default;
  Identifier(const Number id) : id{id} {}

  operator bool() const { return id == 0; }
  operator Number() const { return id; }

  Number id{0};
};

struct element_identifier_tag {};

using element_identifier = Identifier<std::uint64_t, element_identifier_tag>;

class Document {
public:
  virtual ~Document() = default;

  /// \return `true` if the document is editable in any way.
  [[nodiscard]] virtual bool editable() const noexcept = 0;

  /// \param encrypted to ask for encrypted saves.
  /// \return `true` if the document is savable.
  [[nodiscard]] virtual bool savable(bool encrypted) const noexcept = 0;

  /// \param path the destination path.
  virtual void save(const common::Path &path) const = 0;

  /// \param path the destination path.
  /// \param password the encryption password.
  virtual void save(const common::Path &path, const char *password) const = 0;

  /// \return the type of the document.
  [[nodiscard]] virtual DocumentType document_type() const noexcept = 0;

  /// \return the amount of entities in the document
  ///         (e.g. slides, sheets and pages).
  [[nodiscard]] virtual std::uint32_t entry_count() const = 0;

  /// \return the root element of the document.
  [[nodiscard]] virtual element_identifier root_element() const = 0;

  /// \return the first entry element of the document.
  [[nodiscard]] virtual element_identifier first_entry_element() const = 0;

  /// \param element_id the element to query.
  /// \return the type of the element.
  [[nodiscard]] virtual ElementType
  element_type(element_identifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the parent of the element.
  [[nodiscard]] virtual element_identifier
  element_parent(element_identifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the first child of the element.
  [[nodiscard]] virtual element_identifier
  element_first_child(element_identifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the previous sibling of the element.
  [[nodiscard]] virtual element_identifier
  element_previous_sibling(element_identifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the next sibling of the element.
  [[nodiscard]] virtual element_identifier
  element_next_sibling(element_identifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \param property the requested property.
  /// \return the requested optional value.
  [[nodiscard]] virtual std::any
  element_property(element_identifier element_id,
                   ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param property the requested property.
  /// \return the requested optional string.
  ///         - return value must not be null
  ///         - lifetime of the return value only guaranteed until next
  ///           function call on the interface.
  [[nodiscard]] virtual const char *
  element_string_property(element_identifier element_id,
                          ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param property the requested property.
  /// \return the requested integer.
  [[nodiscard]] virtual std::uint32_t
  element_uint32_property(element_identifier element_id,
                          ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param property the requested property.
  /// \return the requested bool.
  [[nodiscard]] virtual bool
  element_bool_property(element_identifier element_id,
                        ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param property the requested property.
  /// \return the requested optional string.
  ///         - return value might be null
  ///         - lifetime of the return value only guaranteed until next
  ///           function call on the interface.
  [[nodiscard]] virtual const char *
  element_optional_string_property(element_identifier element_id,
                                   ElementProperty property) const = 0;

  /// \param element_id the element to query.
  /// \param limit_rows
  /// \param limit_cols
  /// \return the requested table dimensions.
  [[nodiscard]] virtual TableDimensions
  table_dimensions(element_identifier element_id, std::uint32_t limit_rows,
                   std::uint32_t limit_cols) const = 0;

  /// \param element_id the element to query.
  /// \return the requested file object.
  ///         - return value should not be null
  ///         - in case of external images it might be null for now.
  [[nodiscard]] virtual std::shared_ptr<File>
  image_file(element_identifier element_id) const = 0;

  /// \param element_id the element.
  /// \param property the property to set.
  /// \param value the value to set.
  virtual void set_element_property(element_identifier element_id,
                                    ElementProperty property,
                                    const std::any &value) const = 0;

  /// \param element_id the element.
  /// \param property the property to set.
  /// \param value the value to set.
  ///        - lifetime of `value` must be guaranteed until the function
  ///          completes.
  virtual void set_element_string_property(element_identifier element_id,
                                           ElementProperty property,
                                           const char *value) const = 0;

  /// \param element_id the element.
  /// \param property the property to remove.
  virtual void remove_element_property(element_identifier element_id,
                                       ElementProperty property) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
