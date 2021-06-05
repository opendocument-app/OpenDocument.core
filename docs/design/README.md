# Design

## Document abstraction
- in-memory indexed tree structure
- read/write properties with `std::any` values

## Spreadsheet abstraction
- inefficient to store sparse tables densely in-memory
- index by row and column

## Iterators
- use iterators only for immutable objects
  - avoids iterator invalidation, multithreading issues
