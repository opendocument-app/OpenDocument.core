package app.opendocument.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** What {@link Document#recalculate()} did. Mirrors {@code odr::Recalculation}. */
public final class Recalculation {
  private final List<SheetPosition> changed;
  private final List<SheetPosition> circular;
  private final List<SheetPosition> unevaluated;

  /** The lists as the native side states them: three counts, then each position as three ints. */
  static Recalculation fromNative(int[] stated) {
    int at = 3;
    List<List<SheetPosition>> lists = new ArrayList<>();
    for (int list = 0; list < 3; ++list) {
      List<SheetPosition> positions = new ArrayList<>(stated[list]);
      for (int i = 0; i < stated[list]; ++i) {
        positions.add(new SheetPosition(stated[at], stated[at + 1], stated[at + 2]));
        at += 3;
      }
      lists.add(Collections.unmodifiableList(positions));
    }
    return new Recalculation(lists.get(0), lists.get(1), lists.get(2));
  }

  private Recalculation(
      List<SheetPosition> changed, List<SheetPosition> circular, List<SheetPosition> unevaluated) {
    this.changed = changed;
    this.circular = circular;
    this.unevaluated = unevaluated;
  }

  /** The formula cells whose result changed, or that had none before. */
  public List<SheetPosition> changed() {
    return changed;
  }

  /** The cells of a cycle, which have no result. */
  public List<SheetPosition> circular() {
    return circular;
  }

  /** Stale formula cells the evaluator cannot resolve. */
  public List<SheetPosition> unevaluated() {
    return unevaluated;
  }
}
