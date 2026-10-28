#pragma once

#include <odr/definitions.hpp>

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <unordered_map>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::oldms::presentation {
class ElementRegistry;
class StyleRegistry;

/// Parses the presentation into `registry`'s slide/frame/paragraph/span/text
/// elements; fills `style_registry` with the resolved character styles.
ElementIdentifier parse_tree(ElementRegistry &registry,
                             StyleRegistry &style_registry,
                             const abstract::ReadableFilesystem &files);

/// Maps a persist object identifier to its offset in the PowerPoint Document
/// stream ([MS-PPT] 2.3.4).
using PersistDirectory = std::unordered_map<std::uint32_t, std::uint32_t>;

/// The persist object directory of the UserEditAtom chain, with the newest
/// offset per id, and what the newest UserEditAtom refers to ([MS-PPT] 2.1.2).
struct EditChain {
  PersistDirectory directory;
  std::uint32_t doc_persist_id{0};
  /// The optional encryptSessionPersistIdRef ([MS-PPT] 2.3.3).
  std::optional<std::uint32_t> encrypt_session_persist_id;
};

/// Walks the "Current User" stream and the UserEditAtom chain of the
/// "PowerPoint Document" stream. Neither is ever encrypted ([MS-PPT] 2.3.7).
EditChain read_edit_chain(std::istream &current_user, std::istream &document);

/// Whether the presentation is encrypted, from `CurrentUserAtom.headerToken`
/// ([MS-PPT] 2.3.2). Nothing where the `/Current User` stream is missing, too
/// short, or does not hold a CurrentUserAtom: that is not an answer.
[[nodiscard]] std::optional<bool>
password_encrypted(const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::presentation
