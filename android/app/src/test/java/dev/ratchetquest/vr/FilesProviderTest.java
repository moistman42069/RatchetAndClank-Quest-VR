// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import androidx.test.core.app.ApplicationProvider;
import java.io.File;
import java.io.FileNotFoundException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;

/** The game's files in the Files app (FilesProvider, 0.1.18), as OpenRAC's launcher has them. */
@RunWith(RobolectricTestRunner.class)
@org.robolectric.annotation.Config(sdk = 34)
public class FilesProviderTest {
    private Context app;
    private FilesProvider provider;

    @Before
    public void setUp() {
        app = ApplicationProvider.getApplicationContext();
        ProviderInfo info = new ProviderInfo();
        info.authority = FilesProvider.authority(app);
        info.exported = true;
        info.grantUriPermissions = true;
        info.readPermission = info.writePermission = "android.permission.MANAGE_DOCUMENTS";
        provider = Robolectric.buildContentProvider(FilesProvider.class).create(info).get();
    }

    private static List<String> column(Cursor cursor, String name) {
        List<String> values = new ArrayList<>();
        int index = cursor.getColumnIndexOrThrow(name);
        while (cursor.moveToNext()) {
            values.add(cursor.getString(index));
        }
        cursor.close();
        return values;
    }

    @Test
    public void itsPlacesAreTheGamesFoldersAndModFilesIsWhereTheGameKeepsThem() throws Exception {
        assertEquals(List.of("root"), column(provider.queryRoots(null), Root.COLUMN_ROOT_ID));
        assertEquals(List.of("Saves and settings", "Mod files", "Games", "Disc images"),
            column(provider.queryChildDocuments("root", null, (String) null), Document.COLUMN_DISPLAY_NAME));
        // The fork's options.cpp: the mods' files beside the cards (the cards' folder's parent, then mod-data).
        File modFiles = new File(app.getFilesDir(), FilesProvider.MOD_FILES);
        assertEquals(new File(app.getFilesDir(), "share/openrac/memcard/mod-data"), modFiles);
        assertTrue(modFiles.isDirectory());
        File seed = new File(modFiles, "item-randomizer/seed.txt");
        seed.getParentFile().mkdirs();
        Files.write(seed.toPath(), "42".getBytes(StandardCharsets.UTF_8));
        // The deepest folder names a file: Mod files, not Saves and settings.
        assertEquals("mod-data/item-randomizer/seed.txt", FilesProvider.documentId(app, seed));
        assertEquals("saves/memcard", FilesProvider.documentId(app, new File(app.getFilesDir(), "share/openrac/memcard")));
        assertNull(FilesProvider.documentId(app, new File(app.getFilesDir(), "openrac/port")));
        assertEquals(List.of("seed.txt"),
            column(provider.queryChildDocuments("mod-data/item-randomizer", null, (String) null),
                Document.COLUMN_DISPLAY_NAME));
    }

    @Test
    public void theGamesOwnShowFilesOpensTheseFolders() {
        // OpenRAC's GameActivity.showFiles calls its launcher's FilesProvider.Companion.show (0.1.18's
        // OpenRAC): in this app, dev.openrac.launcher's stand-in, which opens this provider at the folder.
        assertTrue(dev.openrac.launcher.FilesProvider.Companion.show(app,
            new File(app.getFilesDir(), "share/openrac/memcard")));
        Intent started = Shadows.shadowOf((Application) app).getNextStartedActivity();
        assertEquals(DocumentsContract.buildDocumentUri(FilesProvider.authority(app), "saves/memcard"),
            started.getData());
    }

    @Test
    public void theFilesAppMakesRenamesAndDeletesInsideItsFoldersOnly() throws Exception {
        provider.queryChildDocuments("root", null, (String) null).close();
        String made = provider.createDocument("discs", "application/octet-stream", "disc.iso");
        assertEquals("discs/disc.iso", made);
        assertEquals("discs/disc (1).iso", provider.createDocument("discs", "application/octet-stream", "disc.iso"));
        String renamed = provider.renameDocument(made, "europe.iso");
        assertEquals("discs/europe.iso", renamed);
        assertTrue(new File(app.getFilesDir(), "discs/europe.iso").isFile());
        provider.deleteDocument(renamed);
        assertFalse(new File(app.getFilesDir(), "discs/europe.iso").exists());
        for (String outside : new String[] {"discs/../openrac", "discs/./x", "discs//x", "nowhere", "games/.."}) {
            try {
                provider.queryDocument(outside, null).close();
                fail(outside + " was let through");
            } catch (FileNotFoundException expected) {
                // no step out of its folders
            }
        }
        try {
            provider.deleteDocument("discs");
            fail("a place's folder was deleted");
        } catch (UnsupportedOperationException expected) {
            // its folders stay
        }
        try {
            provider.createDocument("root", "application/octet-stream", "x");
            fail("a file was made beside its folders");
        } catch (UnsupportedOperationException expected) {
            // the root holds only its folders
        }
    }
}
