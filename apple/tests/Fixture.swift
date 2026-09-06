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

  /// A one-page pdf written to a temporary file, its cross-reference offsets
  /// computed so they are right.
  static func pdf() throws -> String {
    let objects = [
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792]"
        + " /Resources << >> /Contents 4 0 R >>",
      "<< /Length 5 >>\nstream\nBT ET\nendstream",
    ]

    var out = "%PDF-1.7\n"
    var offsets: [Int] = []
    for (index, body) in objects.enumerated() {
      offsets.append(out.utf8.count)
      out += "\(index + 1) 0 obj\n\(body)\nendobj\n"
    }

    let start = out.utf8.count
    out += "xref\n0 \(objects.count + 1)\n0000000000 65535 f \n"
    for offset in offsets {
      out += String(format: "%010d 00000 n \n", offset)
    }
    out += "trailer\n<< /Size \(objects.count + 1) /Root 1 0 R >>\n"
    out += "startxref\n\(start)\n%%EOF\n"

    let url = FileManager.default.temporaryDirectory
      .appendingPathComponent("odr-minimal-\(UUID().uuidString).pdf")
    try out.data(using: .isoLatin1)!.write(to: url)
    return url.path
  }

  private static func path(_ name: String, _ extension: String) throws -> String {
    try XCTUnwrap(
      Bundle.module.url(
        forResource: name, withExtension: `extension`, subdirectory: "Fixtures"),
      "\(name).\(`extension`) is missing from the test bundle"
    ).path
  }
}
