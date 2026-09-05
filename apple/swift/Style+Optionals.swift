import Foundation

/// `NS_SWIFT_NAME` on a `typedef struct` is silently ignored, so the C value
/// types arrive under their ObjC names. Renaming them is exactly the sort of
/// thing this target exists for.
public typealias Color = ODRColor
public typealias TableDimensions = ODRTableDimensions
public typealias TablePosition = ODRTablePosition

// The ObjC layer boxes an absent `std::optional` as `nil` in an `NSNumber` or
// `NSValue`, which is faithful but not how Swift reads. These unbox into real
// optionals of the right type. Same values, no second source of truth.

extension NSNumber {
  fileprivate func asEnum<T: RawRepresentable>(_ type: T.Type) -> T?
  where T.RawValue == Int {
    T(rawValue: intValue)
  }
}

extension NSValue {
  fileprivate var asColor: Color {
    var color = Color(red: 0, green: 0, blue: 0, alpha: 0)
    getValue(&color, size: MemoryLayout<Color>.size)
    return color
  }
}

extension TextStyle {
  public var weight: FontWeight? { fontWeight?.asEnum(FontWeight.self) }
  public var style: FontStyle? { fontStyle?.asEnum(FontStyle.self) }
  public var position: FontPosition? { fontPosition?.asEnum(FontPosition.self) }
  public var isUnderlined: Bool? { fontUnderline?.boolValue }
  public var isStruckThrough: Bool? { fontLineThrough?.boolValue }
  public var color: Color? { fontColor?.asColor }
  public var background: Color? { backgroundColor?.asColor }
}

extension ParagraphStyle {
  public var alignment: TextAlign? { textAlign?.asEnum(TextAlign.self) }
  public var baseDirection: TextDirection? {
    direction?.asEnum(TextDirection.self)
  }
}

extension TableCellStyle {
  public var horizontal: HorizontalAlign? {
    horizontalAlign?.asEnum(HorizontalAlign.self)
  }
  public var vertical: VerticalAlign? { verticalAlign?.asEnum(VerticalAlign.self) }
  public var background: Color? { backgroundColor?.asColor }
  public var rotation: Double? { textRotation?.doubleValue }
}

extension GraphicStyle {
  public var stroke: Color? { strokeColor?.asColor }
  public var fill: Color? { fillColor?.asColor }
  public var vertical: VerticalAlign? { verticalAlign?.asEnum(VerticalAlign.self) }
  public var wrap: TextWrap? { textWrap?.asEnum(TextWrap.self) }
  public var horizontal: HorizontalAlign? {
    horizontalPosition?.asEnum(HorizontalAlign.self)
  }
}

extension PageLayout {
  public var orientation: PrintOrientation? {
    printOrientation?.asEnum(PrintOrientation.self)
  }
  public var background: Color? { backgroundColor?.asColor }
  public var baseDirection: TextDirection? {
    direction?.asEnum(TextDirection.self)
  }
}

extension Frame {
  public var depth: Int32? { zIndex?.int32Value }
}

