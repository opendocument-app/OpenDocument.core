package app.opendocument.core;

import java.util.ArrayList;
import java.util.List;

/**
 * An element in a document. Mirrors {@code odr::Element}. Navigation returns
 * {@code null} where the C++ API returns an empty element; {@code as*}
 * accessors return {@code null} when the element is not of that type.
 *
 * <p>Elements keep the {@link Document} they originate from reachable.
 */
public class Element extends NativeResource {
  Element(long handle, Object owner) {
    super(handle, owner, Element::destroy);
  }

  public ElementType type() {
    return ElementType.fromNative(typeNative(handle()));
  }

  public Element parent() {
    return wrap(parentNative(handle()));
  }

  public Element firstChild() {
    return wrap(firstChildNative(handle()));
  }

  public Element previousSibling() {
    return wrap(previousSiblingNative(handle()));
  }

  public Element nextSibling() {
    return wrap(nextSiblingNative(handle()));
  }

  public boolean isUnique() {
    return isUniqueNative(handle());
  }

  public boolean isSelfLocatable() {
    return isSelfLocatableNative(handle());
  }

  public boolean isEditable() {
    return isEditableNative(handle());
  }

  /** Whether this and {@code other} refer to the same element. */
  public boolean isSame(Element other) {
    try {
      return isSameNative(handle(), other.handle());
    } finally {
      other.keepAlive();
    }
  }

  public DocumentPath documentPath() {
    return new DocumentPath(documentPathNative(handle()));
  }

  public Element navigatePath(DocumentPath path) {
    try {
      return wrap(navigatePathNative(handle(), path.handle()));
    } finally {
      path.keepAlive();
    }
  }

  public List<Element> children() {
    List<Element> result = new ArrayList<>();
    for (Element child = firstChild(); child != null; child = child.nextSibling()) {
      result.add(child);
    }
    return result;
  }

  public TextRoot asTextRoot() {
    long h = asTextRootNative(handle());
    return h == 0 ? null : new TextRoot(h, owner());
  }

  public Slide asSlide() {
    long h = asSlideNative(handle());
    return h == 0 ? null : new Slide(h, owner());
  }

  public Sheet asSheet() {
    long h = asSheetNative(handle());
    return h == 0 ? null : new Sheet(h, owner());
  }

  public Page asPage() {
    long h = asPageNative(handle());
    return h == 0 ? null : new Page(h, owner());
  }

  public SheetCell asSheetCell() {
    long h = asSheetCellNative(handle());
    return h == 0 ? null : new SheetCell(h, owner());
  }

  public MasterPage asMasterPage() {
    long h = asMasterPageNative(handle());
    return h == 0 ? null : new MasterPage(h, owner());
  }

  public LineBreak asLineBreak() {
    long h = asLineBreakNative(handle());
    return h == 0 ? null : new LineBreak(h, owner());
  }

  public Paragraph asParagraph() {
    long h = asParagraphNative(handle());
    return h == 0 ? null : new Paragraph(h, owner());
  }

  public Span asSpan() {
    long h = asSpanNative(handle());
    return h == 0 ? null : new Span(h, owner());
  }

  public Text asText() {
    long h = asTextNative(handle());
    return h == 0 ? null : new Text(h, owner());
  }

  public Link asLink() {
    long h = asLinkNative(handle());
    return h == 0 ? null : new Link(h, owner());
  }

  public Bookmark asBookmark() {
    long h = asBookmarkNative(handle());
    return h == 0 ? null : new Bookmark(h, owner());
  }

  public ListElement asList() {
    long h = asListNative(handle());
    return h == 0 ? null : new ListElement(h, owner());
  }

  public ListItem asListItem() {
    long h = asListItemNative(handle());
    return h == 0 ? null : new ListItem(h, owner());
  }

  public Table asTable() {
    long h = asTableNative(handle());
    return h == 0 ? null : new Table(h, owner());
  }

  public TableColumn asTableColumn() {
    long h = asTableColumnNative(handle());
    return h == 0 ? null : new TableColumn(h, owner());
  }

  public TableRow asTableRow() {
    long h = asTableRowNative(handle());
    return h == 0 ? null : new TableRow(h, owner());
  }

  public TableCell asTableCell() {
    long h = asTableCellNative(handle());
    return h == 0 ? null : new TableCell(h, owner());
  }

  public Frame asFrame() {
    long h = asFrameNative(handle());
    return h == 0 ? null : new Frame(h, owner());
  }

  /**
   * @deprecated Merging into {@link #asFrame()}.
   */
  @Deprecated
  public Rect asRect() {
    long h = asRectNative(handle());
    return h == 0 ? null : new Rect(h, owner());
  }

  /**
   * @deprecated See {@link #asRect()}.
   */
  @Deprecated
  public Line asLine() {
    long h = asLineNative(handle());
    return h == 0 ? null : new Line(h, owner());
  }

  /**
   * @deprecated See {@link #asRect()}.
   */
  @Deprecated
  public Circle asCircle() {
    long h = asCircleNative(handle());
    return h == 0 ? null : new Circle(h, owner());
  }

  /**
   * @deprecated See {@link #asRect()}.
   */
  @Deprecated
  public CustomShape asCustomShape() {
    long h = asCustomShapeNative(handle());
    return h == 0 ? null : new CustomShape(h, owner());
  }

  public Image asImage() {
    long h = asImageNative(handle());
    return h == 0 ? null : new Image(h, owner());
  }

  final Element wrap(long handle) {
    return handle == 0 ? null : new Element(handle, owner());
  }

  final List<Element> wrapAll(long[] handles) {
    List<Element> result = new ArrayList<>(handles.length);
    for (long h : handles) {
      result.add(new Element(h, owner()));
    }
    return result;
  }

  static native void destroy(long handle);

  private native int typeNative(long handle);

  private native long parentNative(long handle);

  private native long firstChildNative(long handle);

  private native long previousSiblingNative(long handle);

  private native long nextSiblingNative(long handle);

  private native boolean isUniqueNative(long handle);

  private native boolean isSelfLocatableNative(long handle);

  private native boolean isEditableNative(long handle);

  private native boolean isSameNative(long handle, long otherHandle);

  private native long documentPathNative(long handle);

  private native long navigatePathNative(long handle, long pathHandle);

  private native long asTextRootNative(long handle);

  private native long asSlideNative(long handle);

  private native long asSheetNative(long handle);

  private native long asPageNative(long handle);

  private native long asSheetCellNative(long handle);

  private native long asMasterPageNative(long handle);

  private native long asLineBreakNative(long handle);

  private native long asParagraphNative(long handle);

  private native long asSpanNative(long handle);

  private native long asTextNative(long handle);

  private native long asLinkNative(long handle);

  private native long asBookmarkNative(long handle);

  private native long asListNative(long handle);

  private native long asListItemNative(long handle);

  private native long asTableNative(long handle);

  private native long asTableColumnNative(long handle);

  private native long asTableRowNative(long handle);

  private native long asTableCellNative(long handle);

  private native long asFrameNative(long handle);

  private native long asRectNative(long handle);

  private native long asLineNative(long handle);

  private native long asCircleNative(long handle);

  private native long asCustomShapeNative(long handle);

  private native long asImageNative(long handle);
}
