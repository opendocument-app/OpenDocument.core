import XCTest

@testable import OdrCore

/// Writes an input inline. Text and CSV need no container, so they can be; the
/// document the rest of the suite runs on is a real one, see `Fixture`.
private func write(_ contents: String, as name: String) throws -> String {
  let directory = FileManager.default.temporaryDirectory
    .appendingPathComponent("odr-tests-\(UUID().uuidString)")
  try FileManager.default.createDirectory(
    at: directory, withIntermediateDirectories: true)
  let path = directory.appendingPathComponent(name)
  try contents.write(to: path, atomically: true, encoding: .utf8)
  return path.path
}

private func temporaryDirectory() throws -> String {
  let directory = FileManager.default.temporaryDirectory
    .appendingPathComponent("odr-tests-\(UUID().uuidString)")
  try FileManager.default.createDirectory(
    at: directory, withIntermediateDirectories: true)
  return directory.path
}

final class LibraryTests: XCTestCase {
  func testLibraryIdentifiesItself() {
    XCTAssertFalse(Odr.identification.isEmpty)
    XCTAssertFalse(Odr.commitHash.isEmpty)
  }
}

final class FileTypeTableTests: XCTestCase {
  func testExtensionsResolveBothWays() {
    XCTAssertEqual(Odr.fileType(extension: "odt"), .openDocumentText)
    XCTAssertEqual(try Odr.extension(fileType: .openDocumentText), "odt")
    XCTAssertEqual(Odr.fileCategory(fileType: .openDocumentText), .document)
  }

  func testUnknownExtensionIsUnknownRatherThanAnError() {
    XCTAssertEqual(Odr.fileType(extension: "definitely-not-a-format"), .unknown)
  }

  /// A format carried by another's extension has none of its own, and must say
  /// so rather than invent one.
  func testCanonicalExtensionOfEncryptedOoxmlThrows() {
    XCTAssertThrowsError(try Odr.extension(fileType: .officeOpenXmlEncrypted))
  }
}

final class TextEncodingTests: XCTestCase {
  func testNamesResolveBothWays() {
    XCTAssertEqual(Odr.string(textEncoding: .windows1252), "windows-1252")
    // case and separators are ignored, so an alias resolves
    XCTAssertEqual(Odr.textEncoding(name: "CP1252"), .windows1252)
    XCTAssertEqual(Odr.textEncoding(name: "definitely-not-an-encoding"), .unknown)
    XCTAssertTrue(Odr.names(textEncoding: .windows1252).contains("windows-1252"))
  }

  /// `unknown` is the one with no name, and says so rather than inventing one.
  func testUnknownHasNoName() {
    XCTAssertNil(Odr.string(textEncoding: .unknown))
  }

  func testAllLeavesOutUnknown() {
    let all = Odr.allTextEncodings.map { TextEncoding(rawValue: $0.intValue) }
    XCTAssertTrue(all.contains(.utf8))
    XCTAssertFalse(all.contains(.unknown))
  }

  func testDecodableIsNarrowerThanNamed() {
    XCTAssertTrue(Odr.isDecodable(textEncoding: .utf8))
    // named so a caller can say what a file is, but not decoded here
    XCTAssertFalse(Odr.isDecodable(textEncoding: .shiftJis))
  }

  func testATextFileReportsItsEncoding() throws {
    let path = try write("hello", as: "plain.txt")
    let decoded = try DecodedFile.decode(path: path)
    XCTAssertEqual((decoded as! TextFile).encoding, .utf8)
  }
}

final class DecodeTests: XCTestCase {
  /// The non-BMP character is the point of the payload: `to_nsstring` converts
  /// UTF-8 to UTF-16, and a 😀 is a surrogate pair on the way out.
  func testDecodesTextFile() throws {
    let contents = "hello odr äöü 😀"
    let path = try write(contents, as: "note.txt")
    let decoded = try DecodedFile.decode(path: path)
    XCTAssertEqual(decoded.fileCategory, .text)
    XCTAssertTrue(decoded.isTextFile)
    XCTAssertEqual(try (decoded as! TextFile).text(), contents)
  }

  /// The failure path has to arrive as a typed Swift error, not a crash and not
  /// a silent nil.
  func testMissingFileThrowsFileNotFound() {
    XCTAssertThrowsError(try DecodedFile.decode(path: "/nope/missing.odt")) {
      error in
      let error = error as NSError
      XCTAssertEqual(error.domain, ODRErrorDomain)
      XCTAssertEqual(error.code, ODRError.fileNotFound.rawValue)
      XCTAssertFalse(error.localizedDescription.isEmpty)
    }
  }

  /// A csv is neither a document file nor a text file: it *holds* a text file,
  /// and `CsvFile.document()` is the other view of the same bytes. Its bytes
  /// are still text, so it stays in `FileCategory.text`.
  func testCsvIsNeitherADocumentNorATextFile() throws {
    let path = try write("a,b\n1,2\n", as: "table.csv")
    let decoded = try DecodedFile.decode(path: path)
    XCTAssertEqual(decoded.fileType, .commaSeparatedValues)
    XCTAssertEqual(decoded.fileCategory, .text)
    XCTAssertFalse(decoded.isDocumentFile)
    XCTAssertFalse(decoded.isTextFile)
    XCTAssertTrue(decoded.isCsvFile)

    let csv = try decoded.asCsvFile()
    XCTAssertEqual(try csv.textFile().text(), "a,b\n1,2\n")
    let root = try XCTUnwrap(try csv.document().rootElement())
    XCTAssertFalse(root is TextRoot)
    XCTAssertNotNil(root.firstDescendant(ofType: Sheet.self))
  }

  func testCsvDelimitersMustBeSingleBytes() throws {
    let path = try write("a;b\nc;d\n", as: "table.csv")
    let options = DecodeOptions()
    options.csv.separator = ";"
    let csv = try DecodedFile.decode(path: path, options: options).asCsvFile()
    let root = try XCTUnwrap(try csv.document().rootElement())
    let sheet = try XCTUnwrap(root.firstDescendant(ofType: Sheet.self))
    XCTAssertEqual(sheet.dimensions.columns, 2)
    options.csv.quote = ""
    XCTAssertNoThrow(try DecodedFile.decode(path: path, options: options))
    for invalid in [";,", "é", "😀"] {
      options.csv.separator = invalid
      XCTAssertThrowsError(try DecodedFile.decode(path: path, options: options), invalid)
      options.csv.separator = ";"
      options.csv.quote = invalid
      XCTAssertThrowsError(try DecodedFile.decode(path: path, options: options), invalid)
      options.csv.quote = nil
    }
  }

  /// `odr::Filesystem::exists("")` throws `std::invalid_argument`. Unguarded,
  /// that crossed into ObjC++ and killed the process; it must be a plain `false`
  /// now. This is a regression test for a crash, not a curiosity.
  func testMalformedPathDoesNotCrash() throws {
    let path = try Fixture.odt()
    let document = try DecodedFile.decode(path: path).asDocumentFile().document()
    let filesystem = try document.filesystem()
    XCTAssertFalse(filesystem.exists(path: ""))
    XCTAssertFalse(filesystem.isFile(path: "relative/path"))
  }
}

final class FileNameTests: XCTestCase {
  func testFileOnDiskIsNamedByItsPath() throws {
    let path = try write("hello", as: "note.txt")
    XCTAssertEqual(try File(path: path).name, "note.txt")
  }

  /// A file inside a package is named by its entry, not by the package.
  func testArchiveEntryIsNamedByItsEntry() throws {
    let archive = try DecodedFile.decode(path: try Fixture.odt(), as: .zip)
      .asArchiveFile().archive()
    let entry = try archive.filesystem.open(path: "/content.xml")
    XCTAssertEqual(entry.name, "content.xml")
  }
}

final class ThumbnailTests: XCTestCase {
  func testDocumentFileCarriesItsThumbnail() throws {
    let file = try DecodedFile.decode(path: try Fixture.odt()).asDocumentFile()
    let thumbnail = try XCTUnwrap(file.thumbnail)
    XCTAssertGreaterThan(try thumbnail.data().count, 0)
  }
}

final class HtmlTests: XCTestCase {
  private func service() throws -> HtmlService {
    let file = try DecodedFile.decode(path: try Fixture.odt())
    return try HtmlTranslator.translate(
      file: file, config: HtmlConfig())
  }

  func testRendersHtml() throws {
    let service = try service()
    let view = try XCTUnwrap(service.views.first)
    var resources: NSArray?
    let html = try view.writeHtml(resources: &resources)
    XCTAssertTrue(html.contains("<html"), "not html: \(html.prefix(80))")
    XCTAssertTrue(html.contains("Landscape"), "the document text is missing")
  }

  /// The renderer's css and js are part of the library, so a document renders
  /// with nothing configured and carries its own styles.
  func testRenderedHtmlCarriesItsOwnStyles() throws {
    let view = try XCTUnwrap(try service().views.first)
    var resources: NSArray?
    let html = try view.writeHtml(resources: &resources)
    XCTAssertTrue(html.contains("<style"), "the html has no stylesheet")
  }

  /// Proves the scope crosses the binding; the C++ suite covers the rest.
  func testEditingScopeReachesTheHtml() throws {
    XCTAssertEqual(HtmlConfig().editingScope, .document)

    let config = HtmlConfig()
    config.editable = true
    config.editingScope = .paragraph

    let file = try DecodedFile.decode(path: try Fixture.odt())
    let service = try HtmlTranslator.translate(file: file, config: config)
    var resources: NSArray?
    let html = try XCTUnwrap(service.views.first).writeHtml(resources: &resources)

    XCTAssertTrue(
      html.contains("data-odr-editing-scope=\"paragraph\""), "the scope did not reach the html")
  }

  /// The C++ suite covers where the floor lands; this only proves the margin
  /// crosses the binding, `nil` sides and all.
  func testMinContentMarginReachesTheHtml() throws {
    let config = HtmlConfig()
    config.minContentMargin = DirectionalMeasure(
      right: nil, top: Measure(string: "12px"), left: Measure(string: "1cm"),
      bottom: nil)

    let file = try DecodedFile.decode(path: try Fixture.odt())
    let service = try HtmlTranslator.translate(
      file: file, config: config)
    var resources: NSArray?
    let html = try XCTUnwrap(service.views.first).writeHtml(resources: &resources)

    XCTAssertTrue(
      html.contains(":root{--odr-min-margin-top:12px;--odr-min-margin-left:1cm;}"),
      "the margin did not reach the html")
    XCTAssertFalse(html.contains("--odr-min-margin-right:"), "an unset side was written")
  }

  /// A view's impl points into its service without owning it, so the view has
  /// to keep the service alive itself — the analogue of
  /// `ElementTreeTests.testElementsKeepTheirDocumentAlive`. Rendering off a
  /// service that only ever existed as a temporary used to segfault.
  func testViewsKeepTheirServiceAlive() throws {
    func viewOnly() throws -> HtmlView {
      try XCTUnwrap(try service().views.first)
    }
    let view = try viewOnly()
    XCTAssertFalse(view.path.isEmpty)
    var resources: NSArray?
    XCTAssertFalse(try view.writeHtml(resources: &resources).isEmpty)
  }

  func testSpreadsheetLimitRoundTripsAndReachesTheHtml() throws {
    let config = HtmlConfig()
    config.spreadsheetLimit = TableDimensions(rows: 2, columns: 1)
    XCTAssertEqual(config.spreadsheetLimit?.rows, 2)
    XCTAssertEqual(config.spreadsheetLimit?.columns, 1)
    config.spreadsheetLimitByContent = false

    // A csv renders as a spreadsheet, so this needs no fixture.
    let path = try write(
      "alpha,beta\ngamma,delta\nepsilon,zeta\n", as: "table.csv")
    let file = try DecodedFile.decode(path: path)
    let service = try HtmlTranslator.translate(
      file: file, config: config)
    var resources: NSArray?
    let html = try XCTUnwrap(service.views.first).writeHtml(resources: &resources)

    XCTAssertTrue(html.contains("alpha"), "the first cell is missing")
    XCTAssertFalse(html.contains("epsilon"), "the row limit did not apply")
    XCTAssertFalse(html.contains("beta"), "the column limit did not apply")

    config.spreadsheetLimit = nil
    XCTAssertNil(config.spreadsheetLimit)
  }

  func testBoxedNumbersRoundTrip() throws {
    let config = HtmlConfig()
    config.spreadsheetCellLimit = 1234
    config.spreadsheetViewportMode = .fitWidth
    config.viewportWidth = 390
    config.initialZoom = 1.5
    config.pageRangeEnd = 7
    config.sheetEditOnClick = false

    XCTAssertEqual(config.spreadsheetCellLimit, 1234)
    XCTAssertEqual(config.spreadsheetViewportMode, .fitWidth)
    XCTAssertEqual(config.viewportWidth, 390)
    XCTAssertEqual(config.initialZoom, 1.5)
    XCTAssertEqual(config.pageRangeEnd, 7)
    XCTAssertEqual(config.sheetEditOnClick, false)

    config.spreadsheetCellLimit = nil
    config.spreadsheetViewportMode = nil
    config.viewportWidth = nil
    config.initialZoom = nil
    config.pageRangeEnd = nil
    config.sheetEditOnClick = nil

    XCTAssertNil(config.spreadsheetCellLimit)
    XCTAssertNil(config.spreadsheetViewportMode)
    XCTAssertNil(config.viewportWidth)
    XCTAssertNil(config.initialZoom)
    XCTAssertNil(config.pageRangeEnd)
    XCTAssertNil(config.sheetEditOnClick)
  }

  func testHostMessageHandlerRoundTrips() {
    let config = HtmlConfig()
    XCTAssertEqual(config.hostMessageHandler, "")

    config.hostMessageHandler = "webkit.messageHandlers.odr.postMessage"
    XCTAssertEqual(
      config.hostMessageHandler, "webkit.messageHandlers.odr.postMessage")
  }

  func testBringOfflineWritesFiles() throws {
    let output = try temporaryDirectory()
    let html = try service().bringOffline(to: output)
    let page = try XCTUnwrap(html.pages.first)
    XCTAssertTrue(FileManager.default.fileExists(atPath: page.path))
  }
}

final class HttpServerTests: XCTestCase {
  /// The only test that exercises the shape the app actually ships: render on
  /// demand, served over loopback into a web view.
  func testServesARenderedView() throws {
    let config = HtmlConfig()
    config.relativeResourcePaths = false
    let service = try HtmlTranslator.translate(
      file: try DecodedFile.decode(path: try Fixture.odt()), config: config)

    let server = HttpServer()
    try server.connect(service, prefix: "doc")
    let handle = try server.serve()
    defer { handle.stop() }

    XCTAssertGreaterThan(handle.port, 0)
    XCTAssertTrue(server.isRunning)

    let view = try XCTUnwrap(service.views.first)
    let url = handle.url(prefix: "doc").appendingPathComponent(view.path)

    let expectation = expectation(description: "served")
    var status = -1
    var bytes = 0
    URLSession.shared.dataTask(with: url) { data, response, _ in
      status = (response as? HTTPURLResponse)?.statusCode ?? -1
      bytes = data?.count ?? 0
      expectation.fulfill()
    }.resume()
    wait(for: [expectation], timeout: 30)

    XCTAssertEqual(status, 200)
    XCTAssertGreaterThan(bytes, 0)
  }

  func testStopIsIdempotent() throws {
    let handle = try HttpServer().serve()
    handle.stop()
    handle.stop()
  }
}

final class ElementTreeTests: XCTestCase {
  private func document() throws -> Document {
    try DecodedFile.decode(path: try Fixture.odt())
      .asDocumentFile().document()
  }

  func testWalksTheTree() throws {
    let root = try XCTUnwrap(try document().rootElement())
    let texts = Array(root.descendants(ofType: Text.self))
    XCTAssertEqual(texts.map(\.content), Fixture.odtText)
    XCTAssertTrue(
      root.descendants.allSatisfy { $0.exists },
      "the walk produced an element that does not exist")
  }

  /// `odr::Element` holds a bare pointer into the document, so an element that
  /// outlives its `Document` reference must still be safe to use.
  func testElementsKeepTheirDocumentAlive() throws {
    func rootOnly() throws -> Element {
      try XCTUnwrap(try document().rootElement())
    }
    let root = try rootOnly()
    XCTAssertNotEqual(root.type, .none)
    XCTAssertFalse(Array(root.descendants).isEmpty)
  }

  /// The receiver comes first, and nothing is walked until it is asked for —
  /// `subtree` reading as an `Array` means it built the whole document to hand
  /// out the root.
  func testSubtreeLeadsWithTheReceiver() throws {
    let root = try XCTUnwrap(try document().rootElement())
    let subtree = Array(root.subtree)
    XCTAssertEqual(subtree.count, Array(root.descendants).count + 1)
    XCTAssertTrue(subtree.first is TextRoot)
    XCTAssertFalse(root.subtree is [Element], "subtree is not lazy")
  }

  func testTypedNavigationReturnsTypedElements() throws {
    let root = try XCTUnwrap(try document().rootElement())
    XCTAssertTrue(root is TextRoot, "root of an odt is a TextRoot, got \(type(of: root))")
    XCTAssertNotNil(root.firstDescendant(ofType: Paragraph.self))
  }
}

final class DocumentSaveTests: XCTestCase {
  private func document() throws -> Document {
    try DecodedFile.decode(path: try Fixture.odt())
      .asDocumentFile().document()
  }

  func testSetStyleMarksARun() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let text = try XCTUnwrap(root.firstDescendant(ofType: Text.self))

    let style = TextStyle()
    style.fontWeight = NSNumber(value: FontWeight.bold.rawValue)
    style.fontUnderline = true
    style.fontSize = Measure(string: "14pt")
    try text.setStyle(style)

    let saved = try XCTUnwrap(try document.saveToMemory())
    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("styled.odt")
    try saved.write(to: path)

    let reloaded = try DecodedFile.decode(path: path.path)
      .asDocumentFile().document()
    let reloadedRoot = try XCTUnwrap(try reloaded.rootElement())
    let styled = try XCTUnwrap(reloadedRoot.firstDescendant(ofType: Text.self)).style
    XCTAssertEqual(styled.fontWeight?.intValue, FontWeight.bold.rawValue)
    XCTAssertEqual(styled.fontUnderline?.boolValue, true)
    XCTAssertEqual(styled.fontSize?.stringValue, "14pt")
    XCTAssertNil(styled.fontStyle)
  }

  func testSetStyleRefusesAFontName() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let text = try XCTUnwrap(root.firstDescendant(ofType: Text.self))

    let style = TextStyle()
    style.fontName = "Comic Sans"
    XCTAssertThrowsError(try text.setStyle(style)) { error in
      XCTAssertEqual((error as NSError).code, ODRError.unsupportedOperation.rawValue)
    }
  }

  func testSetStyleAlignsAParagraph() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let paragraph = try XCTUnwrap(root.firstDescendant(ofType: Paragraph.self))

    let style = ParagraphStyle()
    style.textAlign = NSNumber(value: TextAlign.center.rawValue)
    try paragraph.setStyle(style)

    let saved = try XCTUnwrap(try document.saveToMemory())
    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("aligned.odt")
    try saved.write(to: path)

    let reloaded = try DecodedFile.decode(path: path.path)
      .asDocumentFile().document()
    let reloadedRoot = try XCTUnwrap(try reloaded.rootElement())
    let aligned = try XCTUnwrap(reloadedRoot.firstDescendant(ofType: Paragraph.self)).style
    XCTAssertEqual(aligned.textAlign?.intValue, TextAlign.center.rawValue)
  }

  func testSetParagraphStyleRefusesALineHeight() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let paragraph = try XCTUnwrap(root.firstDescendant(ofType: Paragraph.self))

    let style = ParagraphStyle()
    style.lineHeight = Measure(string: "12pt")
    XCTAssertThrowsError(try paragraph.setStyle(style)) { error in
      XCTAssertEqual((error as NSError).code, ODRError.unsupportedOperation.rawValue)
    }
  }

  func testLocaleIsTheLanguageOfTheDefaultStyle() throws {
    let document = try DecodedFile.decode(path: try Fixture.ods())
      .asDocumentFile().document()
    XCTAssertEqual(document.locale, "en-US")
  }

  func testSetCellStyleFillsACell() throws {
    let document = try DecodedFile.decode(path: try Fixture.ods())
      .asDocumentFile().document()
    let root = try XCTUnwrap(try document.rootElement())
    let sheet = try XCTUnwrap(root.firstDescendant(ofType: Sheet.self))

    let cellStyle = TableCellStyle()
    var yellow = ODRColor(red: 255, green: 255, blue: 0, alpha: 255)
    cellStyle.backgroundColor = NSValue(bytes: &yellow, objCType: "{ODRColor=CCCC}")
    cellStyle.horizontalAlign = NSNumber(value: HorizontalAlign.center.rawValue)
    let textStyle = TextStyle()
    textStyle.fontWeight = NSNumber(value: FontWeight.bold.rawValue)
    try sheet.setStyle(cellStyle, textStyle: textStyle, column: 0, row: 0)

    let saved = try XCTUnwrap(try document.saveToMemory())
    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("styled.ods")
    try saved.write(to: path)

    let reloaded = try DecodedFile.decode(path: path.path)
      .asDocumentFile().document()
    let reloadedRoot = try XCTUnwrap(try reloaded.rootElement())
    let reloadedSheet = try XCTUnwrap(reloadedRoot.firstDescendant(ofType: Sheet.self))
    var fill = ODRColor()
    try XCTUnwrap(reloadedSheet.style(column: 0, row: 0).backgroundColor)
      .getValue(&fill, size: MemoryLayout<ODRColor>.size)
    XCTAssertEqual([fill.red, fill.green, fill.blue], [255, 255, 0])
    let cell = try XCTUnwrap(reloadedSheet.cell(column: 0, row: 0))
    let text = try XCTUnwrap(cell.firstDescendant(ofType: Text.self))
    XCTAssertEqual(text.style.fontWeight?.intValue, FontWeight.bold.rawValue)
  }

  func testInsertAndDeleteRowsMoveTheCells() throws {
    let document = try DecodedFile.decode(path: try Fixture.ods())
      .asDocumentFile().document()
    let root = try XCTUnwrap(try document.rootElement())
    let sheet = try XCTUnwrap(root.firstDescendant(ofType: Sheet.self))
    let text = try XCTUnwrap(
      try XCTUnwrap(sheet.cell(column: 0, row: 0)).firstDescendant(ofType: Text.self)
    ).content

    try sheet.insertRows(at: 0, count: 2)

    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("rows.ods")
    try XCTUnwrap(try document.saveToMemory()).write(to: path)
    let reloaded = try DecodedFile.decode(path: path.path)
      .asDocumentFile().document()
    let reloadedRoot = try XCTUnwrap(try reloaded.rootElement())
    let reloadedSheet = try XCTUnwrap(reloadedRoot.firstDescendant(ofType: Sheet.self))
    let moved = try XCTUnwrap(reloadedSheet.cell(column: 0, row: 2))
    XCTAssertEqual(moved.firstDescendant(ofType: Text.self)?.content, text)

    try reloadedSheet.deleteRows(at: 0, count: 2)
    let back = try XCTUnwrap(reloadedSheet.cell(column: 0, row: 0))
    XCTAssertEqual(back.firstDescendant(ofType: Text.self)?.content, text)
  }

  func testInsertAndDeleteColumnsMoveTheCells() throws {
    let document = try DecodedFile.decode(path: try Fixture.ods())
      .asDocumentFile().document()
    let root = try XCTUnwrap(try document.rootElement())
    let sheet = try XCTUnwrap(root.firstDescendant(ofType: Sheet.self))
    let text = try XCTUnwrap(
      try XCTUnwrap(sheet.cell(column: 0, row: 0)).firstDescendant(ofType: Text.self)
    ).content

    try sheet.insertColumns(at: 0, count: 2)

    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("columns.ods")
    try XCTUnwrap(try document.saveToMemory()).write(to: path)
    let reloaded = try DecodedFile.decode(path: path.path)
      .asDocumentFile().document()
    let reloadedRoot = try XCTUnwrap(try reloaded.rootElement())
    let reloadedSheet = try XCTUnwrap(reloadedRoot.firstDescendant(ofType: Sheet.self))
    let moved = try XCTUnwrap(reloadedSheet.cell(column: 2, row: 0))
    XCTAssertEqual(moved.firstDescendant(ofType: Text.self)?.content, text)

    try reloadedSheet.deleteColumns(at: 0, count: 2)
    let back = try XCTUnwrap(reloadedSheet.cell(column: 0, row: 0))
    XCTAssertEqual(back.firstDescendant(ofType: Text.self)?.content, text)
  }

  func testRecalculateComputesTheStaleFormulas() throws {
    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("formulas.fods")
    try (
      #"<?xml version="1.0" encoding="UTF-8"?>"#
        + #"<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0""#
        + #" xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0""#
        + #" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0""#
        + #" office:version="1.3""#
        + #" office:mimetype="application/vnd.oasis.opendocument.spreadsheet">"#
        + #"<office:body><office:spreadsheet><table:table table:name="s"><table:table-row>"#
        + #"<table:table-cell office:value-type="float" office:value="1"><text:p>1</text:p>"#
        + #"</table:table-cell><table:table-cell table:formula="of:=[.A1]*2""#
        + #" office:value-type="float" office:value="2"><text:p>2</text:p></table:table-cell>"#
        + #"<table:table-cell table:formula="of:=[.C1]+1"/>"#
        + #"</table:table-row></table:table></office:spreadsheet></office:body></office:document>"#
    ).write(to: path, atomically: true, encoding: .utf8)
    let document = try DecodedFile.decode(path: path.path).asDocumentFile().document()

    try document.edit(
      operations: #"{"version":2,"ops":[{"op":"setCell","sheet":0,"column":0,"row":0,"#
        + #""value":{"type":"number","number":5,"text":"5"}}]}"#)
    let result = try document.recalculate()

    XCTAssertEqual(result.changed, [SheetPosition(sheet: 0, column: 1, row: 0)])
    XCTAssertEqual(result.circular, [SheetPosition(sheet: 0, column: 2, row: 0)])
    XCTAssertEqual(result.unevaluated, [])
    XCTAssertEqual(try document.recalculate().changed, [])
  }

  func testSetRowAndColumnStyleReachPastTheCells() throws {
    let document = try DecodedFile.decode(path: try Fixture.ods())
      .asDocumentFile().document()
    let root = try XCTUnwrap(try document.rootElement())
    let sheet = try XCTUnwrap(root.firstDescendant(ofType: Sheet.self))

    let cellStyle = TableCellStyle()
    var yellow = ODRColor(red: 255, green: 255, blue: 0, alpha: 255)
    cellStyle.backgroundColor = NSValue(bytes: &yellow, objCType: "{ODRColor=CCCC}")
    try sheet.setStyle(cellStyle, textStyle: TextStyle(), row: 40)
    try sheet.setStyle(cellStyle, textStyle: TextStyle(), column: 30)

    for (column, row) in [(UInt32(0), UInt32(40)), (30, 90)] {
      var fill = ODRColor()
      try XCTUnwrap(sheet.style(column: column, row: row).backgroundColor)
        .getValue(&fill, size: MemoryLayout<ODRColor>.size)
      XCTAssertEqual([fill.red, fill.green, fill.blue], [255, 255, 0])
    }
  }

  func testSetCellStyleRefusesWhatNoEngineWrites() throws {
    let document = try DecodedFile.decode(path: try Fixture.ods())
      .asDocumentFile().document()
    let root = try XCTUnwrap(try document.rootElement())
    let sheet = try XCTUnwrap(root.firstDescendant(ofType: Sheet.self))

    let cellStyle = TableCellStyle()
    cellStyle.wrapText = true
    XCTAssertThrowsError(
      try sheet.setStyle(cellStyle, textStyle: TextStyle(), column: 0, row: 0)
    ) { error in
      XCTAssertEqual((error as NSError).code, ODRError.unsupportedOperation.rawValue)
    }
  }

  func testSaveToMemoryCarriesAnEdit() throws {
    let document = try self.document()
    XCTAssertTrue(document.isSavable)

    let root = try XCTUnwrap(try document.rootElement())
    let text = try XCTUnwrap(root.firstDescendant(ofType: Text.self))
    try text.setContent("saved to memory")

    let saved = try XCTUnwrap(try document.saveToMemory())
    XCTAssertFalse(saved.isEmpty)

    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("from-memory.odt")
    try saved.write(to: path)

    let reloaded = try DecodedFile.decode(path: path.path)
      .asDocumentFile().document()
    let reloadedRoot = try XCTUnwrap(try reloaded.rootElement())
    XCTAssertTrue(
      reloadedRoot.descendants(ofType: Text.self).contains { $0.content == "saved to memory" })
  }
}

final class DocumentStructuralEditTests: XCTestCase {
  private func document() throws -> Document {
    try DecodedFile.decode(path: try Fixture.odt())
      .asDocumentFile().document()
  }

  func testElementByIdentifierResolvesWhatIdentifierHandedOut() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let run = try XCTUnwrap(root.firstDescendant(ofType: Text.self))

    let found = try XCTUnwrap(document.element(identifier: run.identifier))

    XCTAssertEqual((found as? Text)?.content, run.content)
    XCTAssertNil(document.element(identifier: 999_999))
  }

  func testInsertedRunsSurroundTheAnchor() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let run = try XCTUnwrap(root.firstDescendant(ofType: Text.self))

    let before = try XCTUnwrap(try document.insertText(before: run, text: "before "))
    let after = try XCTUnwrap(try document.insertText(after: run, text: " after"))

    XCTAssertEqual(before.content, "before ")
    XCTAssertEqual(after.content, " after")
    XCTAssertEqual(
      root.descendants(ofType: Text.self).prefix(3).map(\.content),
      ["before ", Fixture.odtText[0], " after"])
  }

  func testAnAddedParagraphTakesAnAddedRun() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let first = try XCTUnwrap(root.firstDescendant(ofType: Paragraph.self))

    let added = try XCTUnwrap(try document.insertParagraph(after: first))
    let run = try XCTUnwrap(try document.appendText(to: added, text: "a new paragraph"))

    XCTAssertEqual(run.content, "a new paragraph")
    XCTAssertTrue(
      root.descendants(ofType: Text.self).contains { $0.content == "a new paragraph" })
  }

  func testRemoveTakesTheElementOut() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let run = try XCTUnwrap(root.firstDescendant(ofType: Text.self))

    try document.remove(run)

    // The fixture repeats its runs, so what proves the removal is the sequence.
    XCTAssertEqual(
      root.descendants(ofType: Text.self).map(\.content),
      Array(Fixture.odtText.dropFirst()))
  }

  func testSplitAndMergeAreInverse() throws {
    let document = try self.document()
    let root = try XCTUnwrap(try document.rootElement())
    let first = try XCTUnwrap(root.firstDescendant(ofType: Paragraph.self))
    let run = try XCTUnwrap(first.firstDescendant(ofType: Text.self))

    _ = try document.splitParagraph(first, after: run)
    try document.mergeParagraphWithNext(first)

    XCTAssertEqual(root.descendants(ofType: Text.self).map(\.content), Fixture.odtText)
  }
}

final class TextFileEditTests: XCTestCase {
  func testWritesAnEditBack() throws {
    let path = try write("hello text file\n", as: "note.txt")
    let file = try DecodedFile.decode(path: path).asTextFile()
    XCTAssertTrue(file.isSavable)

    let edited = try XCTUnwrap(
      try file.writeEdited(
        operations: #"{"version":2,"ops":[{"op":"setContent","text":"rewritten"}]}"#))

    XCTAssertEqual(String(data: edited, encoding: .utf8), "rewritten")
  }

  func testEditsAndSavesLikeADocument() throws {
    let path = try write("hello text file\n", as: "note.txt")
    let file = try DecodedFile.decode(path: path).asTextFile()

    try file.edit(
      operations: #"{"version":2,"ops":[{"op":"setContent","text":"rewritten"}]}"#)

    XCTAssertEqual(try file.text(), "rewritten")
    XCTAssertEqual(String(data: try file.saveToMemory(), encoding: .utf8), "rewritten")
    let saved = (path as NSString).deletingLastPathComponent + "/saved.txt"
    try file.save(to: saved)
    XCTAssertEqual(try String(contentsOfFile: saved, encoding: .utf8), "rewritten")
  }
}

final class PdfAnnotationTests: XCTestCase {
  private static let highlight = """
    {"version": 1, "annotations": [{"page": 0, "type": "highlight",
     "quads": [[72, 700, 300, 700, 72, 688, 300, 688]],
     "color": [1, 0.9, 0.2]}]}
    """

  func testAnnotateIsDeclaredForPdf() throws {
    let capabilities = Odr.capabilities(fileType: .portableDocumentFormat)
    XCTAssertTrue(capabilities.annotate)
  }

  func testAnnotateAppendsToTheSource() throws {
    let path = try Fixture.pdf()
    let source = try Data(contentsOf: URL(fileURLWithPath: path))

    let file = try DecodedFile.decode(path: path).asPdfFile()
    let result = try file.annotate(Self.highlight)

    XCTAssertGreaterThan(result.count, source.count)
    // the source is copied through and the annotation written after it
    XCTAssertEqual(result.prefix(source.count), source)

    let text = String(decoding: result, as: UTF8.self)
    XCTAssertTrue(text.contains("/Highlight"))
    XCTAssertTrue(text.contains("/Subtype /Form"))
  }

  func testAnnotateRefusesAPayloadItDoesNotUnderstand() throws {
    let file = try DecodedFile.decode(path: try Fixture.pdf()).asPdfFile()
    XCTAssertThrowsError(try file.annotate("{\"version\": 2}"))
    XCTAssertThrowsError(try file.annotate("not json"))
  }
}

final class MeasureTests: XCTestCase {
  func testConstructionReturnsNilOnFailure() throws {
    let parsed = try XCTUnwrap(Measure(string: "2.5cm"))
    XCTAssertEqual(parsed.magnitude, 2.5)
    XCTAssertEqual(parsed.unit, "cm")
    XCTAssertEqual(Measure(magnitude: 2.5, unit: "cm").stringValue, "2.5cm")
    XCTAssertEqual(Measure(string: "12")?.unit, "")
    for invalid in ["", "invalid", "1e999px"] {
      XCTAssertNil(Measure(string: invalid), invalid)
    }
  }
}

final class TableAddressTests: XCTestCase {
  func testRoundTrips() throws {
    XCTAssertEqual(TableAddress.columnNumber(from: "C"), 2)
    XCTAssertEqual(TableAddress.rowNumber(from: "5"), 4)
    XCTAssertEqual(TableAddress.string(fromColumn: 2), "C")
    var position = TablePosition(column: 0, row: 0)
    try TableAddress.position(&position, from: "C5")
    XCTAssertEqual(position.column, 2)
    XCTAssertEqual(position.row, 4)
  }
}

final class DocumentCreateTests: XCTestCase {
  func testCreateMakesEveryTypeThatStatesCreate() throws {
    for number in Odr.allFileTypes {
      let type = try XCTUnwrap(FileType(rawValue: number.intValue))
      guard Odr.capabilities(fileType: type).create else { continue }
      let document = try Document.create(fileType: type)
      XCTAssertEqual(document.fileType, type)
      XCTAssertTrue(document.isEditable)
    }
  }

  func testCreateRefusesATypeWithoutCreate() {
    XCTAssertThrowsError(try Document.create(fileType: .officeOpenXmlPresentation)) { error in
      XCTAssertEqual((error as NSError).code, ODRError.unsupportedFileType.rawValue)
    }
  }

  func testACreatedDocumentTakesAnEditThroughASave() throws {
    let document = try Document.create(fileType: .openDocumentText)
    let root = try XCTUnwrap(try document.rootElement())
    let paragraph = try XCTUnwrap(root.firstDescendant(ofType: Paragraph.self))
    XCTAssertNotNil(try document.appendText(to: paragraph, text: "written in swift"))

    let path = URL(fileURLWithPath: try temporaryDirectory())
      .appendingPathComponent("created.odt")
    try XCTUnwrap(try document.saveToMemory()).write(to: path)

    let reloaded = try DecodedFile.decode(path: path.path).asDocumentFile().document()
    let reloadedRoot = try XCTUnwrap(try reloaded.rootElement())
    XCTAssertEqual(
      reloadedRoot.descendants(ofType: Text.self).map { $0.content }, ["written in swift"])
  }
}
