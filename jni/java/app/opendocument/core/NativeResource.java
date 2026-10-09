package app.opendocument.core;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.LongConsumer;

/**
 * Owns a native object, freed by {@link #close()} or garbage collection.
 * Navigation results retain their owner; closing it invalidates them.
 * A {@link PhantomReference} reaper supports Android API 26.
 */
public abstract class NativeResource implements AutoCloseable {
  private static final ReferenceQueue<NativeResource> QUEUE = new ReferenceQueue<>();

  /** Keeps phantom references reachable until their native objects are freed. */
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
    if (owner instanceof NativeResource) {
      ((NativeResource) owner).handle();
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

  /**
   * Call after passing this object's handle as a native argument to prevent mid-call collection.
   * Replaces {@code Reference.reachabilityFence}, unavailable before Android API 28.
   */
  final void keepAlive() {
    synchronized (this) {
      // empty on purpose - taking the monitor is the use
    }
  }

  /** Frees the native object early. Idempotent. */
  @Override
  public void close() {
    closed = true;
    destroyer.destroy();
  }

  /** Frees the native object at most once, without retaining its wrapper. */
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
