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

  /// A csv is a *text* file to odrcore, not a document file. It does have an
  /// element tree — `CsvFile.document()` is a second view of the same bytes —
  /// but that does not move it out of `FileCategory.text`.
  func testCsvIsTextRatherThanADocument() throws {
    let path = try write("a,b\n1,2\n", as: "table.csv")
    let decoded = try DecodedFile.decode(path: path)
    XCTAssertEqual(decoded.fileType, .commaSeparatedValues)
    XCTAssertEqual(decoded.fileCategory, .text)
    XCTAssertFalse(decoded.isDocumentFile)
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

final class HtmlTests: XCTestCase {
  private func service() throws -> HtmlService {
    let file = try DecodedFile.decode(path: try Fixture.odt())
    return try HtmlTranslator.translate(
      file: file, cachePath: try temporaryDirectory(), config: HtmlConfig())
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

  /// The C++ suite covers where the floor lands; this only proves the margin
  /// crosses the binding, `nil` sides and all.
  func testMinContentMarginReachesTheHtml() throws {
    let config = HtmlConfig()
    config.minContentMargin = DirectionalMeasure(
      right: nil, top: Measure(string: "12px"), left: Measure(string: "1cm"),
      bottom: nil)

    let file = try DecodedFile.decode(path: try Fixture.odt())
    let service = try HtmlTranslator.translate(
      file: file, cachePath: try temporaryDirectory(), config: config)
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
      file: file, cachePath: try temporaryDirectory(), config: config)
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

    XCTAssertEqual(config.spreadsheetCellLimit, 1234)
    XCTAssertEqual(config.spreadsheetViewportMode, .fitWidth)
    XCTAssertEqual(config.viewportWidth, 390)
    XCTAssertEqual(config.initialZoom, 1.5)
    XCTAssertEqual(config.pageRangeEnd, 7)

    config.spreadsheetCellLimit = nil
    config.spreadsheetViewportMode = nil
    config.viewportWidth = nil
    config.initialZoom = nil
    config.pageRangeEnd = nil

    XCTAssertNil(config.spreadsheetCellLimit)
    XCTAssertNil(config.spreadsheetViewportMode)
    XCTAssertNil(config.viewportWidth)
    XCTAssertNil(config.initialZoom)
    XCTAssertNil(config.pageRangeEnd)
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
      file: try DecodedFile.decode(path: try Fixture.odt()),
      cachePath: try temporaryDirectory(), config: config)

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
