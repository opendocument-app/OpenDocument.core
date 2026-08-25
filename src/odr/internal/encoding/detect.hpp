#pragma once

#include <odr/file.hpp>

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>

namespace odr::internal::encoding {

/// Enough for uchardet, and bounded so classifying a large file costs no pass
/// over it.
constexpr std::size_t default_probe_size = std::size_t{64} * 1024;

/// Reads at most @p max_bytes from @p in, from wherever it stands.
[[nodiscard]] std::string
read_probe(std::istream &in, std::size_t max_bytes = default_probe_size);

/// The encoding of @p probe, @ref TextEncoding::unknown if it cannot be named.
/// A byte-order mark decides alone, else uchardet guesses — from the probe
/// only, so an encoding invisible in an ASCII prefix is missed.
[[nodiscard]] TextEncoding detect(std::string_view probe);

/// Whether @p probe, detected as @p encoding, reads as text: named, and no NUL
/// outside utf-16 and utf-32 — the test `file(1)` and git use. Empty is text.
[[nodiscard]] bool is_text(std::string_view probe, TextEncoding encoding);

} // namespace odr::internal::encoding
