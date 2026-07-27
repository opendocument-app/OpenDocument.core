package app.opendocument.core;

/**
 * A log sink. Implement this and wrap it in a {@link Logger} to route odr's
 * diagnostics. Mirrors {@code odr::ILogger}.
 *
 * <p>{@link Logger} gates on {@link #willLog} before calling {@link #log}, so
 * an implementation may assume the level reaching {@code log} is enabled.
 *
 * <p>Methods may be invoked from native worker threads, so implementations must
 * be thread-safe.
 */
public interface ILogger {
  boolean willLog(LogLevel level);

  /**
   * @param epochMillis time of the message, in milliseconds since the epoch
   */
  void log(long epochMillis, LogLevel level, String message, SourceLocation location);

  void flush();
}
