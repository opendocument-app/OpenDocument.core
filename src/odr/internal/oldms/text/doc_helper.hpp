#pragma once

#include <algorithm>
#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <vector>

namespace odr::internal::oldms::text {

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

} // namespace odr::internal::oldms::text
