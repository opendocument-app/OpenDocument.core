import Foundation
import XCTest

/// The document the suite runs on.
///
/// `Fixtures/mixed-layout.odt` is `odt/mixed-layout.odt` from
/// [OpenDocument.test](https://github.com/opendocument-app/OpenDocument.test),
/// copied in rather than referenced. `test/data/` is fetched by
/// `cmake/test_data.cmake` and is not part of a package checkout, and pulling
/// it in as a submodule is precisely what `Package.swift` must not do — SwiftPM
/// initialises submodules on every consumer's checkout.
///
/// 9 KB of real LibreOffice output, four paragraphs across three master pages,
/// each a text run plus a span. It replaced a document this suite wrote itself,
/// which only ever proved that odrcore could read back what the test had
/// written.
enum Fixture {
  /// The text nodes of `odt`, in document order. Each paragraph is a run and a
  /// span, so the numbers are their own nodes.
  static let odtText = [
    "Portrait ", "1", "Portrait ", "2", "Landscape ", "1", "Portrait ", "3",
  ]

  static func odt() throws -> String {
    try path("mixed-layout", "odt")
  }

  private static func path(_ name: String, _ extension: String) throws -> String {
    try XCTUnwrap(
      Bundle.module.url(
        forResource: name, withExtension: `extension`, subdirectory: "Fixtures"),
      "\(name).\(`extension`) is missing from the test bundle"
    ).path
  }
}
