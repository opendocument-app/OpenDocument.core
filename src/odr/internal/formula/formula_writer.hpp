#pragma once

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_parser.hpp>

#include <string>

namespace odr::internal::formula {

/// Writes @p node back in @p syntax, without the `of:=` prefix or the leading
/// `=`. A parenthesis the precedence already states is dropped.
[[nodiscard]] std::string to_string(const Node &node, Syntax syntax);

} // namespace odr::internal::formula
