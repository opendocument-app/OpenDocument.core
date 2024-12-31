#pragma once

#include <memory>
#include <vector>

namespace odr::internal::pdf {

struct Catalog;
struct Element;

struct Document {
  Catalog *catalog;
  std::vector<std::unique_ptr<Element>> elements;

  template <typename T, typename... Args> T *create_element(Args &&...args) {
    auto unique = std::make_unique<T>(std::forward<Args>(args)...);
    auto pointer = unique.get();
    elements.push_back(std::move(unique));
    return pointer;
  }
};

} // namespace odr::internal::pdf
