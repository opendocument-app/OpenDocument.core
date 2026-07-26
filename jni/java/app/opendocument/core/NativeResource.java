package app.opendocument.core;

import java.lang.ref.Cleaner;
import java.util.function.LongConsumer;

/**
 * Owns a handle to a heap-allocated native object. The native object is freed
 * on {@link #close()} or, at the latest, when the garbage collector reclaims
 * this wrapper (via {@link Cleaner}).
 *
 * <p>Navigation results (e.g. document elements) keep the object they
 * originate from reachable through {@code owner}, so a root object is not
 * collected while handles into it are alive.
 */
public abstract class NativeResource implements AutoCloseable {
  private static final Cleaner CLEANER = Cleaner.create();

  private final long handle;
  private final Object owner;
  private final Cleaner.Cleanable cleanable;
  private volatile boolean closed;

  NativeResource(long handle, Object owner, LongConsumer destroyer) {
    this.handle = handle;
    this.owner = owner;
    this.cleanable = CLEANER.register(this, new Destroyer(handle, destroyer));
  }

  /** The native handle; valid only while this object is open. */
  final long handle() {
    if (closed) {
      throw new IllegalStateException(getClass().getSimpleName() + " is closed");
    }
    return handle;
  }

  final Object owner() {
    return owner;
  }

  /** Frees the native object early. Idempotent. */
  @Override
  public void close() {
    closed = true;
    cleanable.clean();
  }

  private record Destroyer(long handle, LongConsumer destroy) implements Runnable {
    @Override
    public void run() {
      destroy.accept(handle);
    }
  }
}
