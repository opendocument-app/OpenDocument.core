package app.opendocument.core.android

import android.content.Context
import android.content.res.AssetManager
import app.opendocument.core.GlobalParams
import app.opendocument.core.Odr
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

/**
 * Makes the library usable on android: the renderer reads its CSS/JS and the libmagic database as
 * plain files, and an APK holds them as assets, which are not files.
 *
 * Call [init] once before anything else touches the library:
 * ```
 * OdrAndroid.init(context)
 * val file = Odr.open(path)
 * ```
 */
object OdrAndroid {
    /** Asset directory this AAR ships its runtime data under. */
    private const val ASSETS = "core"

    /** The libmagic database alone is 8 MB, so the default 8 KB would be a lot of syscalls. */
    private const val BUFFER_SIZE = 64 * 1024

    private var initialized = false

    /**
     * Extracts the bundled runtime data and points the library at it. Repeated calls are cheap: the
     * data is extracted once per version of the library and reused afterwards.
     *
     * @param context any context; only the app's private storage and its assets are used
     * @throws IOException if the assets cannot be extracted
     */
    @JvmStatic
    @Synchronized
    @Throws(IOException::class)
    fun init(context: Context) {
        if (initialized) {
            return
        }

        // Keyed by the exact library build: an app that updates must not keep
        // reading the assets of the version it had before, and there is no reason
        // to back these up or restore them onto another device.
        val root = File(context.noBackupFilesDir, "odr-core/${Odr.commitHash()}")
        val marker = File(root, ".complete")
        if (!marker.isFile) {
            root.deleteRecursively()
            extract(context.assets, ASSETS, root)
            if (!marker.createNewFile()) {
                throw IOException("could not mark $root as complete")
            }
        }

        GlobalParams.setOdrCoreDataPath(File(root, "odrcore").absolutePath)
        GlobalParams.setLibmagicDatabasePath(File(root, "libmagic/magic.mgc").absolutePath)
        initialized = true
    }

    /** An asset directory lists its children; an asset file lists nothing. */
    private fun extract(assets: AssetManager, path: String, target: File) {
        val children = assets.list(path)
        if (children.isNullOrEmpty()) {
            copy(assets, path, target)
            return
        }
        if (!target.isDirectory && !target.mkdirs()) {
            throw IOException("could not create $target")
        }
        for (child in children) {
            extract(assets, "$path/$child", File(target, child))
        }
    }

    private fun copy(assets: AssetManager, path: String, target: File) {
        val parent = target.parentFile
        if (parent != null && !parent.isDirectory && !parent.mkdirs()) {
            throw IOException("could not create $parent")
        }
        assets.open(path).use { input ->
            FileOutputStream(target).use { output -> input.copyTo(output, BUFFER_SIZE) }
        }
    }
}
