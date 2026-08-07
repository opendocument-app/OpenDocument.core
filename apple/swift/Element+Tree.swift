import Foundation

extension Element {
  /// Every descendant, depth first, excluding the receiver.
  ///
  /// Lazily produced — walking a large document to find the first match should
  /// not materialise the whole tree.
  public var descendants: some Sequence<Element> {
    DepthFirstSequence(children)
  }

  /// The receiver and every descendant, depth first.
  public var subtree: some Sequence<Element> {
    DepthFirstSequence([self])
  }

  /// Every descendant of the given type, depth first.
  ///
  ///     for text in root.descendants(ofType: Text.self) { ... }
  public func descendants<T: Element>(ofType type: T.Type) -> some Sequence<T> {
    descendants.lazy.compactMap { $0 as? T }
  }

  /// The first descendant of the given type, or `nil`.
  public func firstDescendant<T: Element>(ofType type: T.Type) -> T? {
    descendants(ofType: type).first { _ in true }
  }

  /// The chain of ancestors, closest first.
  public var ancestors: some Sequence<Element> {
    sequence(first: self) { $0.parent }.dropFirst()
  }
}

/// Depth first, iterative rather than recursive: a deeply nested document
/// should not be able to overflow the stack of whoever iterates it.
private struct DepthFirstSequence: Sequence, IteratorProtocol {
  private var stack: [Element]

  /// `pending` in document order — the stack pops from the back.
  init(_ pending: [Element]) {
    stack = pending.reversed()
  }

  mutating func next() -> Element? {
    guard let element = stack.popLast() else { return nil }
    stack.append(contentsOf: element.children.reversed())
    return element
  }
}

extension TextRoot {
  /// The document's text, in reading order.
  public var text: String {
    descendants(ofType: Text.self).map(\.content).joined()
  }
}
