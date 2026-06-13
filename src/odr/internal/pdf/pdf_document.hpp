#pragma once

#include <memory>
#include <vector>

namespace odr::internal::pdf {

struct Catalog;
struct Element;
struct Page;

struct Document {
  Catalog *catalog{nullptr};
  std::vector<std::unique_ptr<Element>> elements;

  template <typename T, typename... Args> T *create_element(Args &&...args) {
    auto unique = std::make_unique<T>(std::forward<Args>(args)...);
    auto pointer = unique.get();
    elements.push_back(std::move(unique));
    return pointer;
  }

  /// Depth-first flatten of the page tree rooted at `pages` into its leaf
  /// `Page`s, in reading order. Throws on an unexpected element type — a page
  /// tree contains only `Pages` and `Page` nodes (ISO 32000-1 7.7.3.2).
  std::vector<Page *> collect_pages() const;
};

} // namespace odr::internal::pdf
