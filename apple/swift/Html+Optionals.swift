import Foundation

// `HtmlConfig`'s optional settings are `NS_REFINED_FOR_SWIFT`, so the
// `NSNumber`/`NSValue` boxes are `__`-prefixed and these carry the names.

extension HtmlConfig {
  public var spreadsheetLimit: TableDimensions? {
    get { __spreadsheetLimit?.tableDimensionsValue }
    set {
      __spreadsheetLimit = newValue.map { NSValue.value(tableDimensions: $0) }
    }
  }

  public var spreadsheetCellLimit: UInt64? {
    get { __spreadsheetCellLimit?.uint64Value }
    set { __spreadsheetCellLimit = newValue.map(NSNumber.init(value:)) }
  }

  public var spreadsheetViewportMode: HtmlViewportMode? {
    get {
      __spreadsheetViewportMode.flatMap {
        HtmlViewportMode(rawValue: $0.intValue)
      }
    }
    set {
      __spreadsheetViewportMode = newValue.map { NSNumber(value: $0.rawValue) }
    }
  }

  public var viewportWidth: UInt32? {
    get { __viewportWidth?.uint32Value }
    set { __viewportWidth = newValue.map(NSNumber.init(value:)) }
  }

  public var initialZoom: Double? {
    get { __initialZoom?.doubleValue }
    set { __initialZoom = newValue.map(NSNumber.init(value:)) }
  }

  public var pageRangeEnd: UInt32? {
    get { __pageRangeEnd?.uint32Value }
    set { __pageRangeEnd = newValue.map(NSNumber.init(value:)) }
  }
}
