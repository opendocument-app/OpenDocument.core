import Foundation
import XCTest

/// The document the suite runs on: `odt/mixed-layout.odt` from
/// [OpenDocument.test](https://github.com/opendocument-app/OpenDocument.test),
/// copied in because `test/data/` is fetched and a package checkout has none of
/// it — and a submodule is the one thing `Package.swift` must never grow.
///
/// 9 KB of real LibreOffice output: four paragraphs across three master pages,
/// each a text run plus a span.
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
