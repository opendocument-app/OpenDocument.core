package app.opendocument.core;

import java.util.function.LongConsumer;

/**
 * Keeps guarded native calls valid during concurrent {@link #close()}.
 * Closing rejects new calls, invokes {@link #unblock()}, waits, then frees the handle.
 * Guarded calls must return after unblocking, or closing waits forever.
 * Concurrent close is unsupported for unguarded calls.
 */
public abstract class GuardedNativeResource extends NativeResource {
  /** Guards {@link #inFlight} and {@link #closing}. */
  private final Object lock = new Object();

  /** Guarded calls that have taken the handle and not handed it back yet. */
  private int inFlight;

  /** Set by {@link #close()} before it frees anything, so no call starts after. */
  private boolean closing;

  GuardedNativeResource(long handle, Object owner, LongConsumer destroyer) {
    super(handle, owner, destroyer);
  }

  /**
   * Runs {@code call} on the native handle and holds it valid for the duration.
   * Does nothing if the resource is closing or closed.
   */
  protected final void guarded(LongConsumer call) {
    synchronized (lock) {
      if (closing) {
        return;
      }
      inFlight++;
    }
    try {
      call.accept(handle());
    } finally {
      synchronized (lock) {
        inFlight--;
        lock.notifyAll();
      }
    }
  }

  /**
   * Makes the guarded calls in flight return; called by {@link #close()} before it
   * waits for them. Does nothing by default, which only suits calls that end on
   * their own.
   */
  protected void unblock() {}

  /** Frees the native object once no guarded call is on it any more. Idempotent. */
  @Override
  public synchronized void close() {
    if (isClosed()) {
      return;
    }

    synchronized (lock) {
      closing = true;
    }

    unblock();

    boolean interrupted = false;
    synchronized (lock) {
      while (inFlight > 0) {
        try {
          lock.wait();
        } catch (InterruptedException e) {
          // giving up here would leak the native object instead, and the wait is
          // bounded by unblock() above
          interrupted = true;
        }
      }
    }
    if (interrupted) {
      Thread.currentThread().interrupt();
    }

    super.close();
  }
}
