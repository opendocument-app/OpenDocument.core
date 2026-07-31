import XCTest

@testable import OdrCore

/// Inputs are built inline rather than shipped as fixtures, the same rule the
/// JNI suite follows. Text and CSV need no container, so they can be written
/// with `String.write` — which keeps the test target free of a zip writer.
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

final class BootstrapTests: XCTestCase {
  /// The whole point of the dynamic framework: `+load` pointed odrcore at the
  /// bundled resources before `main`, with nothing in this test calling it. If
  /// this fails, every rendering test fails too, but for a reason that would be
  /// much harder to read off.
  func testResourcesAreWiredUpWithoutAnyoneAskingFor() {
    let path = GlobalParams.odrCoreDataPath
    XCTAssertFalse(path.isEmpty, "odr core data path was never set")
    XCTAssertTrue(
      FileManager.default.fileExists(atPath: path + "/document.css"),
      "\(path) does not contain the renderer's css")
  }

  func testLibmagicDatabaseIsBundled() {
    let path = GlobalParams.libmagicDatabasePath
    XCTAssertTrue(
      FileManager.default.fileExists(atPath: path),
      "libmagic database missing at \(path)")
  }

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
  func testDecodesTextFile() throws {
    let path = try write("hello odr", as: "note.txt")
    let decoded = try DecodedFile.decode(path: path)
    XCTAssertEqual(decoded.fileCategory, .text)
    XCTAssertTrue(decoded.isTextFile)
    XCTAssertEqual(try (decoded as! TextFile).text(), "hello odr")
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

  /// `odr::Filesystem::exists("")` throws `std::invalid_argument`. Unguarded,
  /// that crossed into ObjC++ and killed the process; it must be a plain `false`
  /// now. This is a regression test for a crash, not a curiosity.
  func testMalformedPathDoesNotCrash() throws {
    let path = try write("a,b\n1,2\n", as: "table.csv")
    let document = try DecodedFile.decode(path: path).asDocumentFile().document()
    let filesystem = try document.filesystem()
    XCTAssertFalse(filesystem.exists(path: ""))
    XCTAssertFalse(filesystem.isFile(path: "relative/path"))
  }
}

final class HtmlTests: XCTestCase {
  private func service() throws -> HtmlService {
    let path = try write("a,b\n1,2\n", as: "table.csv")
    let file = try DecodedFile.decode(path: path)
    let config = HtmlConfig()
    // odrcore rejects relative resource paths when the output is served
    config.relativeResourcePaths = false
    return try HtmlTranslator.translate(
      file: file, cachePath: try temporaryDirectory(), config: config)
  }

  func testRendersHtml() throws {
    let service = try service()
    let view = try XCTUnwrap(service.views.first)
    var resources: NSArray?
    let html = try view.writeHtml(resources: &resources)
    XCTAssertTrue(html.contains("<html"), "not html: \(html.prefix(80))")
    XCTAssertTrue(html.contains("<table"), "the csv did not become a table")
  }

  /// The default config must already point at the framework's own resources.
  func testDefaultConfigUsesBundledResources() {
    XCTAssertEqual(HtmlConfig().resourcePath, GlobalParams.odrCoreDataPath)
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
    let path = try write("a,b\n1,2\n", as: "table.csv")
    let config = HtmlConfig()
    config.relativeResourcePaths = false
    let service = try HtmlTranslator.translate(
      file: try DecodedFile.decode(path: path),
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
    let path = try write("a,b\n1,2\n", as: "table.csv")
    return try DecodedFile.decode(path: path).asDocumentFile().document()
  }

  func testWalksTheTree() throws {
    let root = try XCTUnwrap(try document().rootElement())
    let cells = Array(root.descendants(ofType: SheetCell.self))
    XCTAssertFalse(cells.isEmpty, "no cells in a csv")
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

  func testTypedNavigationReturnsTypedElements() throws {
    let root = try XCTUnwrap(try document().rootElement())
    let sheet = root.firstDescendant(ofType: Sheet.self)
    XCTAssertNotNil(sheet, "a csv should have a sheet")
    XCTAssertGreaterThan(sheet?.dimensions.rows ?? 0, 0)
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
