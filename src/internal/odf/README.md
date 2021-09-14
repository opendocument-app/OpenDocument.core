# ODF implementation

## shortcomings

- some properties allow relative value adoption (e.g. first `font-size:10pt` then `font-size:200%` which effectively sets `font-size:20pt`)
  - CSS does not support such behaviour, needs to be solved while translation

## references

- http://docs.oasis-open.org/office/v1.2/os/OpenDocument-v1.2-os-part1.html
