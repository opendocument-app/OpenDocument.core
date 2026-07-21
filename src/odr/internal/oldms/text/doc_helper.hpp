#pragma once

#include <odr/internal/oldms/text/doc_structs.hpp>

#include <algorithm>
#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <vector>

namespace odr::internal::oldms::text {

class StyleRegistry;

class CharacterIndex {
public:
  [[nodiscard]] bool empty() const { return m_entries.empty(); }
  [[nodiscard]] std::size_t size() const { return m_entries.size(); }

  [[nodiscard]] std::size_t last_cp() const {
    if (m_entries.empty()) {
      return 0;
    }
    return m_entries.back().end_cp;
  }

  void append(const std::size_t start_cp, const std::size_t length_cp,
              const std::size_t data_offset, const bool is_compressed) {
    const std::size_t end_cp = start_cp + length_cp;
    if (end_cp < last_cp()) {
      throw std::runtime_error(
          "append must be used in order of increasing start_cp");
    }
    m_entries.emplace_back(end_cp, data_offset, is_compressed);
  }

  struct Entry {
    std::size_t start_cp;
    std::size_t length_cp;
    std::size_t data_offset;
    std::size_t data_length;
    bool is_compressed;
  };

private:
  struct InternalEntry;

public:
  struct Iterator {
    Iterator(const CharacterIndex &parent, const std::size_t index)
        : m_parent(&parent), m_index(index) {}

    Entry operator*() const {
      const std::size_t start_cp = m_index == 0 ? 0 : prev().end_cp;
      const std::size_t length_cp = entry().end_cp - start_cp;
      const std::size_t data_length =
          length_cp * (entry().is_compressed ? 1 : 2);

      return Entry{start_cp, length_cp, entry().data_offset, data_length,
                   entry().is_compressed};
    }

    Iterator &operator++() {
      ++m_index;
      return *this;
    }

    Iterator operator++(int) {
      const Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    bool operator==(const Iterator &other) const {
      return m_index == other.m_index;
    }

  private:
    const CharacterIndex *m_parent{nullptr};
    std::size_t m_index{0};

    [[nodiscard]] const InternalEntry &entry() const {
      return m_parent->m_entries[m_index];
    }
    [[nodiscard]] const InternalEntry &prev() const {
      return m_parent->m_entries[m_index - 1];
    }
  };

  [[nodiscard]] Iterator begin() const { return {*this, 0}; }
  [[nodiscard]] Iterator end() const { return {*this, m_entries.size()}; }
  [[nodiscard]] Iterator find(const std::size_t cp) const {
    const auto it =
        std::ranges::lower_bound(m_entries, cp, {}, &InternalEntry::end_cp);
    if (it == m_entries.end()) {
      return end();
    }
    return {*this, static_cast<std::size_t>(it - m_entries.begin())};
  }

private:
  struct InternalEntry {
    std::size_t end_cp;
    std::size_t data_offset{};
    bool is_compressed{};
  };

  std::vector<InternalEntry> m_entries;
};

CharacterIndex read_character_index(std::istream &in);

/// The document's character-formatting runs, keyed by WordDocument-stream
/// offset (fc), each referencing a `StyleRegistry` style index. Index 0 is
/// the default style; offsets outside any run resolve to it.
class CharacterRuns final {
public:
  /// Appends a run; runs must be ascending and non-overlapping. Adjacent runs
  /// with the same style index are merged.
  void append_run(std::uint32_t begin_fc, std::uint32_t end_fc,
                  std::uint32_t style_index);

  /// The style index of the run containing `fc` (0 if none).
  [[nodiscard]] std::uint32_t index_at(std::uint32_t fc) const;
  /// End offset of the run containing `fc`, or the next run's begin, so text
  /// can be walked in style-uniform chunks; UINT32_MAX past the last run.
  [[nodiscard]] std::uint32_t chunk_end(std::uint32_t fc) const;

private:
  struct Run {
    std::uint32_t begin_fc;
    std::uint32_t end_fc;
    std::uint32_t style_index;
  };

  std::vector<Run> m_runs;
};

/// Reads the PlcBteChpx at `fc` in the table stream and every referenced
/// ChpxFkp page ([MS-DOC] 2.9.33) from the WordDocument stream, resolving
/// each run's Chpx on top of the default style (index 0) into
/// `style_registry`. Equal Chpx bytes share one style.
CharacterRuns read_character_runs(std::istream &document_stream,
                                  std::istream &table_stream,
                                  FcLcb plcf_bte_chpx,
                                  StyleRegistry &style_registry,
                                  const std::vector<const char *> &font_names);

} // namespace odr::internal::oldms::text
