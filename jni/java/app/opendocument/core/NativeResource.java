package app.opendocument.core;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.LongConsumer;

/**
 * Owns a handle to a heap-allocated native object. The native object is freed
 * on {@link #close()} or, at the latest, when the garbage collector reclaims
 * this wrapper.
 *
 * <p>Navigation results (e.g. document elements) keep the object they
 * originate from reachable through {@code owner}, so a root object is not
 * collected while handles into it are alive.
 *
 * <p>The post-mortem free is a {@link PhantomReference} drained by a daemon
 * thread rather than a {@link java.lang.ref.Cleaner}, which is exactly what a
 * Cleaner does internally. Cleaner is a JDK 9 API that android only ships from
 * API level 33, and OpenDocument.droid targets API 26; referencing it made the
 * whole class fail to load there with a {@code NoClassDefFoundError}, and core
 * library desugaring does not cover {@code java.lang.ref}.
 */
public abstract class NativeResource implements AutoCloseable {
  private static final ReferenceQueue<NativeResource> QUEUE = new ReferenceQueue<>();

  /**
   * Keeps the pending {@link Destroyer}s reachable. A phantom reference that is
   * itself collected never gets enqueued, so the native object behind it would
   * leak instead of being freed.
   */
  private static final Set<Destroyer> PENDING = ConcurrentHashMap.newKeySet();

  static {
    Thread reaper =
        new Thread(
            () -> {
              while (true) {
                try {
                  ((Destroyer) QUEUE.remove()).destroy();
                } catch (InterruptedException e) {
                  Thread.currentThread().interrupt();
                  return;
                } catch (RuntimeException | Error e) {
                  // one broken destructor must not stop the others from running
                }
              }
            },
            "odr-core-native-resource-reaper");
    reaper.setDaemon(true);
    reaper.start();
  }

  private final long handle;
  private final Object owner;
  private final Destroyer destroyer;
  private volatile boolean closed;

  NativeResource(long handle, Object owner, LongConsumer destroyer) {
    this.handle = handle;
    this.owner = owner;
    this.destroyer = new Destroyer(this, handle, destroyer);
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

  /** Whether the native object has been freed already. */
  final boolean isClosed() {
    return closed;
  }

  /** Frees the native object early. Idempotent. */
  @Override
  public void close() {
    closed = true;
    destroyer.destroy();
  }

  /**
   * Frees one native object, at most once, whether it is reached through
   * {@link #close()} or through the reference queue.
   *
   * <p>Holds no reference to the {@link NativeResource} other than the phantom
   * one - a strong one would keep the wrapper alive forever.
   */
  private static final class Destroyer extends PhantomReference<NativeResource> {
    private final long handle;
    private final LongConsumer destroy;
    private final AtomicBoolean destroyed = new AtomicBoolean();

    Destroyer(NativeResource referent, long handle, LongConsumer destroy) {
      super(referent, QUEUE);

      this.handle = handle;
      this.destroy = destroy;

      PENDING.add(this);
    }

    void destroy() {
      if (!destroyed.compareAndSet(false, true)) {
        return;
      }

      PENDING.remove(this);
      clear();
      destroy.accept(handle);
    }
  }
}
