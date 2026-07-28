package app.opendocument.core;

import android.content.Context;
import androidx.test.platform.app.InstrumentationRegistry;
import app.opendocument.core.android.OdrAndroid;
import java.io.File;
import java.io.IOException;
import java.nio.file.Path;

/** Shared setup of the instrumented suite: the initialised library and a scratch directory. */
final class TestSupport {
  static Context context() {
    return InstrumentationRegistry.getInstrumentation().getTargetContext();
  }

  /** The library, with its bundled assets extracted and registered. */
  static void initialize() throws IOException {
    OdrAndroid.init(context());
  }

  /** An empty directory under the app cache, named after the caller. */
  static Path tempDir(String name) throws IOException {
    File directory = new File(context().getCacheDir(), name);
    delete(directory);
    if (!directory.mkdirs()) {
      throw new IOException("could not create " + directory);
    }
    return directory.toPath();
  }

  private static void delete(File file) {
    File[] children = file.listFiles();
    if (children != null) {
      for (File child : children) {
        delete(child);
      }
    }
    file.delete();
  }

  private TestSupport() {}
}
