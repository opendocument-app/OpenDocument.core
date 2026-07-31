import Foundation

/// Builds a minimal ODT in memory.
///
/// The suite stays hermetic — no fixture files, the same rule
/// `jni/testfixtures` follows, where the equivalent is a `ZipOutputStream`.
/// Swift has no zip writer, but an ODT does not need compression: every entry
/// is *stored*, which is a header, the bytes, a central directory and an end
/// record. That also satisfies ODF's requirement that `mimetype` come first and
/// uncompressed.
///
/// Flat ODF (`.fodt`) would avoid the zip entirely, but odrcore does not detect
/// a hand-written one by content, so this is the smaller surprise.
enum MinimalOdt {
  static func write(text: String = "hello odr") throws -> String {
    let content = """
      <?xml version="1.0" encoding="UTF-8"?>
      <office:document-content \
      xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" \
      xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" \
      office:version="1.3">\
      <office:body><office:text><text:p>\(text)</text:p></office:text></office:body>\
      </office:document-content>
      """
    let manifest = """
      <?xml version="1.0" encoding="UTF-8"?>
      <manifest:manifest \
      xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" \
      manifest:version="1.3">\
      <manifest:file-entry manifest:full-path="/" \
      manifest:media-type="application/vnd.oasis.opendocument.text"/>\
      <manifest:file-entry manifest:full-path="content.xml" \
      manifest:media-type="text/xml"/>\
      </manifest:manifest>
      """

    let archive = zip([
      ("mimetype", Data("application/vnd.oasis.opendocument.text".utf8)),
      ("content.xml", Data(content.utf8)),
      ("META-INF/manifest.xml", Data(manifest.utf8)),
    ])

    let directory = FileManager.default.temporaryDirectory
      .appendingPathComponent("odr-odt-\(UUID().uuidString)")
    try FileManager.default.createDirectory(
      at: directory, withIntermediateDirectories: true)
    let path = directory.appendingPathComponent("minimal.odt")
    try archive.write(to: path)
    return path.path
  }

  private static func zip(_ entries: [(name: String, data: Data)]) -> Data {
    var archive = Data()
    var directory = Data()

    for entry in entries {
      let name = Data(entry.name.utf8)
      let crc = crc32(entry.data)
      let offset = UInt32(archive.count)

      // local file header, stored
      archive.append(contentsOf: [0x50, 0x4B, 0x03, 0x04])
      archive.append(uint16(20))  // version needed
      archive.append(uint16(0))  // flags
      archive.append(uint16(0))  // method: stored
      archive.append(uint16(0))  // time
      archive.append(uint16(0x21))  // date: 1980-01-01
      archive.append(uint32(crc))
      archive.append(uint32(UInt32(entry.data.count)))
      archive.append(uint32(UInt32(entry.data.count)))
      archive.append(uint16(UInt16(name.count)))
      archive.append(uint16(0))  // extra length
      archive.append(name)
      archive.append(entry.data)

      directory.append(contentsOf: [0x50, 0x4B, 0x01, 0x02])
      directory.append(uint16(20))  // version made by
      directory.append(uint16(20))  // version needed
      directory.append(uint16(0))
      directory.append(uint16(0))
      directory.append(uint16(0))
      directory.append(uint16(0x21))
      directory.append(uint32(crc))
      directory.append(uint32(UInt32(entry.data.count)))
      directory.append(uint32(UInt32(entry.data.count)))
      directory.append(uint16(UInt16(name.count)))
      directory.append(uint16(0))  // extra
      directory.append(uint16(0))  // comment
      directory.append(uint16(0))  // disk
      directory.append(uint16(0))  // internal attributes
      directory.append(uint32(0))  // external attributes
      directory.append(uint32(offset))
      directory.append(name)
    }

    let directoryOffset = UInt32(archive.count)
    archive.append(directory)
    archive.append(contentsOf: [0x50, 0x4B, 0x05, 0x06])
    archive.append(uint16(0))  // this disk
    archive.append(uint16(0))  // disk with directory
    archive.append(uint16(UInt16(entries.count)))
    archive.append(uint16(UInt16(entries.count)))
    archive.append(uint32(UInt32(directory.count)))
    archive.append(uint32(directoryOffset))
    archive.append(uint16(0))  // comment length
    return archive
  }

  private static func uint16(_ value: UInt16) -> Data {
    withUnsafeBytes(of: value.littleEndian) { Data($0) }
  }

  private static func uint32(_ value: UInt32) -> Data {
    withUnsafeBytes(of: value.littleEndian) { Data($0) }
  }

  private static let table: [UInt32] = (0..<256).map { index -> UInt32 in
    (0..<8).reduce(UInt32(index)) { value, _ in
      value & 1 == 1 ? 0xEDB8_8320 ^ (value >> 1) : value >> 1
    }
  }

  private static func crc32(_ data: Data) -> UInt32 {
    ~data.reduce(~UInt32(0)) { crc, byte in
      table[Int((crc ^ UInt32(byte)) & 0xFF)] ^ (crc >> 8)
    }
  }
}
