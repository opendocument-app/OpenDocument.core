package app.opendocument.core.android;

import android.content.Context;
import android.content.res.AssetManager;
import app.opendocument.core.GlobalParams;
import app.opendocument.core.Odr;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Makes the library usable on android: the renderer reads its CSS/JS and the libmagic database as
 * plain files, and an APK holds them as assets, which are not files.
 *
 * <p>Call {@link #init} once before anything else touches the library:
 *
 * <pre>{@code
 * OdrAndroid.init(context);
 * DecodedFile file = Odr.open(path);
 * }</pre>
 */
public final class OdrAndroid {
  /** Asset directory this AAR ships its runtime data under. */
  private static final String ASSETS = "core";

  private static boolean initialized;

  /**
   * Extracts the bundled runtime data and points the library at it. Repeated calls are cheap: the
   * data is extracted once per version of the library and reused afterwards.
   *
   * @param context any context; only the app's private storage and its assets are used
   * @throws IOException if the assets cannot be extracted
   */
  public static synchronized void init(Context context) throws IOException {
    if (initialized) {
      return;
    }

    // Keyed by the exact library build: an app that updates must not keep
    // reading the assets of the version it had before, and there is no reason
    // to back these up or restore them onto another device.
    File root = new File(context.getNoBackupFilesDir(), "odr-core/" + Odr.commitHash());
    File marker = new File(root, ".complete");
    if (!marker.isFile()) {
      deleteRecursively(root);
      extract(context.getAssets(), ASSETS, root);
      if (!marker.createNewFile()) {
        throw new IOException("could not mark " + root + " as complete");
      }
    }

    GlobalParams.setOdrCoreDataPath(new File(root, "odrcore").getAbsolutePath());
    GlobalParams.setLibmagicDatabasePath(new File(root, "libmagic/magic.mgc").getAbsolutePath());
    initialized = true;
  }

  private static void extract(AssetManager assets, String path, File target) throws IOException {
    String[] children = assets.list(path);
    if (children == null || children.length == 0) {
      copy(assets, path, target);
      return;
    }
    if (!target.isDirectory() && !target.mkdirs()) {
      throw new IOException("could not create " + target);
    }
    for (String child : children) {
      extract(assets, path + "/" + child, new File(target, child));
    }
  }

  private static void copy(AssetManager assets, String path, File target) throws IOException {
    File parent = target.getParentFile();
    if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
      throw new IOException("could not create " + parent);
    }
    byte[] buffer = new byte[1 << 16];
    try (InputStream input = assets.open(path);
        OutputStream output = new FileOutputStream(target)) {
      for (int read = input.read(buffer); read != -1; read = input.read(buffer)) {
        output.write(buffer, 0, read);
      }
    }
  }

  private static void deleteRecursively(File file) {
    File[] children = file.listFiles();
    if (children != null) {
      for (File child : children) {
        deleteRecursively(child);
      }
    }
    file.delete();
  }

  private OdrAndroid() {}
}
