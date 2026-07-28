package app.opendocument.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** The AAR's own contract: the native library loads and the bundled assets end up on disk. */
@RunWith(AndroidJUnit4.class)
public class OdrAndroidTest {
  @Before
  public void setUp() throws IOException {
    TestSupport.initialize();
  }

  @Test
  public void nativeLibraryLoads() {
    // reaching the native side at all means the .so, its ABI and libc++_shared
    // are all where the packaging put them
    assertFalse(Odr.identify().isEmpty());
    assertNotNull(Odr.version());
    assertNotNull(Odr.commitHash());
  }

  @Test
  public void assetsAreExtracted() {
    File data = new File(GlobalParams.odrCoreDataPath());
    assertTrue(data + " is not a directory", data.isDirectory());
    assertTrue(new File(data, "document.css").isFile());
    assertTrue(new File(data, "document.js").isFile());

    File magic = new File(GlobalParams.libmagicDatabasePath());
    assertTrue(magic + " is not a file", magic.isFile());
  }

  @Test
  public void initIsIdempotent() throws IOException {
    String dataPath = GlobalParams.odrCoreDataPath();
    TestSupport.initialize();
    assertEquals(dataPath, GlobalParams.odrCoreDataPath());
  }

  @Test
  public void detectsTypeWithTheBundledMagicDatabase() throws IOException {
    Path directory = TestSupport.tempDir("magic");
    Path odt = TestFiles.odtFile(directory);

    // goes through libmagic, so it only works when the database extracted above
    // is the one the native library actually opened
    assertEquals("application/vnd.oasis.opendocument.text", Odr.mimetype(odt.toString()));
  }
}
