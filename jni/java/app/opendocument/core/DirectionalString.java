package app.opendocument.core;

/** Per-direction strings (e.g. borders). Any side may be {@code null}. */
public final class DirectionalString {
  public final String right;
  public final String top;
  public final String left;
  public final String bottom;

  public DirectionalString(String right, String top, String left, String bottom) {
    this.right = right;
    this.top = top;
    this.left = left;
    this.bottom = bottom;
  }
}
