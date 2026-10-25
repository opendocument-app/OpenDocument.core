# The formula evaluator without exceptions

Status: in progress. Related: [`spreadsheet-editing.md`](spreadsheet-editing.md)
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

The evaluator returns its stops as values, and the module builds with
`-fno-exceptions`, which is smaller and faster than both modes.

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
3. **The module.** `require()` and the catches go. A profile
   `emscripten-wasm-no-eh` builds the module and its dependencies with
   `-fno-exceptions`; `formula.yml` uses it, and a `throw` in the formula code
   then fails the build. The JS exception profile of #1283 goes.

## Left out

The rest of core throws too (941 `throw`s): its parsers report a broken file
that way. The npm package keeps native wasm exceptions, and states the
browsers it needs: Chrome 95, Safari 15.2, Firefox 100.
