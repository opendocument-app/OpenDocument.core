#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace odr::internal::rtf {

/// The group stack of *Conventions of an RTF Reader*: `{` stores the current
/// state, `}` retrieves it. Never empty, so an unmatched `}` is ignored and
/// the state the document started from always remains.
class State final {
public:
  struct Group final {
    /// The group's text is not body text — a control destination, or a `{\*`
    /// group whose destination we do not implement.
    bool discard{false};
    /// `\ucN`: how many fallback characters follow each `\uN`.
    std::int32_t uc{1};
  };

  State();

  [[nodiscard]] Group &current();
  [[nodiscard]] const Group &current() const;

  void save();
  /// An unmatched `}` is ignored.
  void restore();

  [[nodiscard]] std::size_t depth() const noexcept;

private:
  /// A group stack is heap, so this is not about stack overflow: it fails
  /// fast on input that nests far past anything a writer produces.
  static constexpr std::size_t max_depth = 1024;

  std::vector<Group> m_stack;
};

} // namespace odr::internal::rtf
