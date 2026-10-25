# The formula evaluator without exceptions

Status: done. Related: [`spreadsheet-editing.md`](spreadsheet-editing.md)
decision 5, which builds the evaluator as the formula module of a page.

## Problem

The native builds embed the evaluator as `odr-formula.js`, which an app runs
in whatever WebView the device has. The evaluator throws, so the module needs
an exception mode:

- **Native wasm exceptions** (`-fwasm-exceptions`) need Chrome 95. A WebView
  before it refuses the whole module (`unexpected section <Exception>`), so
  7.6.1 refused every typed formula there.
- **Exceptions in JavaScript** (`-fexceptions`, #1283) load anywhere, but the
  module is 741 KB, not 640 KB, and an evaluation takes 2 to 4 times as long.

Catching at the boundary does not help: every function a `throw` may pass
through carries the instructions of the mode.

The evaluator throws as control flow, not for faults: 129 `throw NoAnswer{}`
(no answer both applications agree on) and 8 `throw ErrorResult{}` (an
argument error that becomes the result of its function). Two places catch
them: a function call (`ErrorResult`) and `evaluate` (`NoAnswer`).

## Decision

The evaluator returns its stops as values, and the module builds without
exception handling. It loads anywhere, and is as small and as fast as the
module with native exceptions: 646 KB, and 5 µs for `A1+A2*3`.

- **One result type.** `Stop` is what ends an evaluation early: no answer, or
  an error that becomes the result of the function. `Result<T>` is
  `std::expected<T, Stop>`.
- **Explicit propagation, no macro.** A call site checks the result and
  returns its stop (`if (!x) { return std::unexpected(x.error()); }`). The
  monadic calls of `std::expected` (`and_then`, `transform`) are fine where
  they read naturally. No macro hides a `return`.
- **Visitors return whether to go on.** `CellSource::for_each_cell` and
  `Call::for_each` take a visitor that returns a `Result<void>`; a stop ends
  the walk and comes back as the result of the walk. The two sources that
  override `for_each_cell` (`sheet_cell_source.cpp`, `sheet_recalculation.cpp`)
  follow.
- **Unchanged outside.** `formula::evaluate` and `evaluate_array` keep their
  `std::optional`, so the recalculation and the wasm binding do not change.
- **No throwing call of the standard library** in the module: checked access
  instead of `.at()`, `.value()` and `std::get`, and `std::from_chars` instead
  of `std::stod`. With `-fno-exceptions` such a call aborts.
- **Every function is `noexcept`**: the functions, the evaluator, the `Call`
  accessors, the helpers, and the pointer types `Function` and `ArrayRule`.
  An exception that still reaches one ends the program: a failed allocation,
  or a `CellSource` that throws, which a native document's adapter can.
  `src/odr/internal/formula/.clang-tidy` names the exceptions of the standard
  library that `bugprone-exception-escape` does not count, and why.

## Order of work

Each step builds and passes the suite.

1. **The core.** `Stop`, `Result`, the evaluator, the `Call` accessors and the
   helpers of `formula_function.hpp` return results. A function that is not
   converted yet unwraps a result with `require()`, a temporary bridge that
   throws the stop, and the evaluator still catches it. `require()` marks
   what is left.
2. **The functions, one PR for each file:** math, logic, text, date, lookup.
   A function returns `Result<Value>`, and its `throw`s and `require()`s
   become returns.
3. **The module.** `require()` and the catches go. `formula.yml` builds the
   module with the profile `emscripten-wasm-no-eh`, which replaces the JS
   exception profile of #1283.

## How the module builds

- **The formula code** is its own library, `odr_formula_oz`, built with
  `-fno-exceptions`, and so is `wasm/formula/odr_formula.cpp`. A `throw` or a
  `catch` there fails every wasm build, the npm one too.
- **The rest it links** (`odr_oz`: the number formats, the string helpers)
  still throws, so it cannot build with `-fno-exceptions`. The profile builds
  it and the dependencies with `-fignore-exceptions`, which emscripten picks
  without a flag: a `throw` raises a JavaScript
  exception that nothing in the module catches, and the module carries no
  exception code. `scripts/wasm_has_no_native_eh.py` checks that.
- **A format code** that does not parse is `number_format::Format::parse`
  giving nothing, where the module caught `std::invalid_argument` before.
- **`TextCursor::advance`** throws out of line, so the formula parser can
  include its header.

## Left out

The rest of core throws too (941 `throw`s): its parsers report a broken file
that way. The npm package keeps native wasm exceptions, and states the
browsers it needs: Chrome 95, Safari 15.2, Firefox 100.
