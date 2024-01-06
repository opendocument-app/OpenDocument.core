# PDF

This is in a very early state and not tested thoroughly.

It includes:
- Reading PDF data types from stream.
- Opening a PDF file and extracting file objects.
  - Everything is done via streams but some parts require seeking in the file.
  - **What is missing:**
    - Support for linearized files
    - Support for cross-reference stream
    - Updated files?
- Extracting information from the PDF file objects.
  - We can construct a page tree and link to annotations and resources.
  - **What is missing:**
    - Properly parse annotation information
    - Properly parse font information

## Discussion: HTML translation

The primer problem of translating PDF to HTML will be to map glyph IDs to Unicode characters.
There can be mapping information in the PDF and in the font but both is not guaranteed.
HTML and SVG do not support drawing characters based on glyph IDs, so we are forced to use Unicode mappings.
If there is no Unicode mapping in the font we would have to transform the font and make up mappings.
This would require the ability of reading and writing font files which may not be trivial and there are a couple of different formats with different versions.

Ultimately it also depends on how often this really happens that we do not have any Unicode mappings at all.

Another problem might be the font support from the browser. Googling suggests that not all formats are supported by popular browsers.

After all this is solved the questions is if we can place text the same way in HTML as it is done in PDF.
PDF has the ability to continue text where it left of or move down a line which could be tricky in HTML.
Apart from that PDF can also modify the spacing between the characters for example which we might be able to translate to CSS positioning.

## References
- https://en.wikipedia.org/wiki/PDF
- File Format
  - Objects https://www.oreilly.com/library/view/developing-with-pdf/9781449327903/ch01.html
  - https://resources.infosecinstitute.com/topics/hacking/pdf-file-format-basic-structure/
- Structure
  - Document Structure https://www.oreilly.com/library/view/pdf-explained/9781449321581/ch04.html
  - Cross-reference stream https://www.verypdf.com/document/pdf-format-reference/pg_0106.htm
- Graphics Operators
  - https://gendignoux.com/blog/images/pdf-graphics/cheat-sheet-by-nc-sa.png 
  - https://pdfa.org/wp-content/uploads/2023/08/PDF-Operators-CheatSheet.pdf
  - General https://gendignoux.com/blog/2017/01/05/pdf-graphics.html
  - Text https://www.syncfusion.com/succinctly-free-ebooks/pdf/text-operators
  - Drawing https://www.syncfusion.com/succinctly-free-ebooks/pdf/graphics-operators
- Related projects
  - https://github.com/qpdf/qpdf
  - https://github.com/caradoc-org/caradoc
- Nasty test files https://github.com/angea/PDF101
