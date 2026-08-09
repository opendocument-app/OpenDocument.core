package app.opendocument.core;

/** List element. Mirrors {@code odr::List}; named apart from {@code java.util.List}. */
public final class ListElement extends Element {
  ListElement(long handle, Object owner) {
    super(handle, owner);
  }

  /** Named apart from {@link Element#type}, which every element answers. */
  public ListType listType() {
    return ListType.fromNative(listTypeNative(handle()));
  }

  private native int listTypeNative(long handle);
}
