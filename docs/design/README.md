# Design

## Motivation

- non-intrusive read/write access to documents
  - saving should not depend on our internal representation of the document but only the changes
- support of different formats thought one API
- lightweight API
- handle document specifics behind the API
- value semantics for the user facing API

## Document abstraction

## Spreadsheet abstraction

- inefficient to store sparse tables densely in-memory

## Iterators

- try to use iterators only for immutable objects
  - avoids iterator invalidation, multithreading issues

## Future

### Document index

- replace iterators where it makes sense with an index
- challenges
  - some document formats "compress" information (e.g. ods columns/rows/cells or xlsx strings).
    these elements need to be inflated before indexing to guarantee correctness
    (counter example: editing a cell that is repeated would destroy the other cells index)
