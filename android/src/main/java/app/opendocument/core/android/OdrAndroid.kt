package app.opendocument.core.android

import android.content.Context
import java.io.IOException

/**
 * Android entry point of the library.
 *
 * Nothing needs setting up any more: the renderer's CSS/JS are part of the native library, so the
 * assets this used to extract are gone. [init] stays as a no-op so existing callers keep working.
 */
object OdrAndroid {
    /**
     * Does nothing.
     *
     * @param context unused
     */
    @Deprecated("The library needs no setup; this does nothing.")
    @JvmStatic
    @Synchronized
    @Throws(IOException::class)
    fun init(context: Context) {}
}
