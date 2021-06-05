# Design

## Motivation
- non-intrusive read/write access to documents
  - saving should not depend on our internal representation of the document but only the changes
- support of different formats thought one API
- lightweight API
- handle document specifics behind the API
- value semantics for the user facing API

## Document abstraction
- in-memory indexed tree structure
- read/write properties with `std::any` values

## Spreadsheet abstraction
- inefficient to store sparse tables densely in-memory
- index by row and column

## Iterators
- use iterators only for immutable objects
  - avoids iterator invalidation, multithreading issues
