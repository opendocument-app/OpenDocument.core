package app.opendocument.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A presentation: its slides. Mirrors {@code odr::Presentation}. */
public final class Presentation extends Document {
  Presentation(long handle) {
    super(handle);
  }

  /**
   * The layouts {@link #insertSlide} starts a slide from, each the empty slide it makes, outside
   * the document and refusing edits; empty where the engine cannot insert a slide.
   */
  public List<Slide> slideLayouts() {
    long[] handles = slideLayoutsNative(handle());
    List<Slide> result = new ArrayList<>(handles.length);
    for (long layout : handles) {
      result.add(new Slide(layout, this));
    }
    return Collections.unmodifiableList(result);
  }

  /**
   * An empty slide at {@code index} among the slides, as the {@code layout}-th of {@link
   * #slideLayouts}.
   */
  public Slide insertSlide(int index, int layout) {
    return new Slide(insertSlideNative(handle(), index, layout), this);
  }

  /** Puts {@code slide} at {@code index} among the slides, counted after the move. */
  public void moveSlide(Slide slide, int index) {
    try {
      moveSlideNative(handle(), slide.handle(), index);
    } finally {
      slide.keepAlive();
    }
  }

  /**
   * A text box with one empty paragraph on a slide, drawing no fill and no outline. Every length
   * needs a fixed unit, and the size must be positive.
   */
  public Frame insertTextBox(Element page, Measure x, Measure y, Measure width, Measure height) {
    try {
      return new Frame(insertTextBoxNative(handle(), page.handle(), x, y, width, height), this);
    } finally {
      page.keepAlive();
    }
  }

  private native long[] slideLayoutsNative(long handle);

  private native long insertSlideNative(long handle, int index, int layout);

  private native void moveSlideNative(long handle, long slideHandle, int index);

  private native long insertTextBoxNative(
      long handle, long pageHandle, Measure x, Measure y, Measure width, Measure height);
}
