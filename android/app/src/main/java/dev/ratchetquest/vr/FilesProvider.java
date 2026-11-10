// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.os.StatFs;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;
import android.webkit.MimeTypeMap;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * The game's files in the system's Files app (0.1.18, as OpenRAC's launcher has them from 65ee2bdc: its
 * launcher/src-tauri/gen/android/.../FilesProvider.kt, in Java): "Ratchet & Clank Quest VR" in its list of places, with
 * the space left. The files stay in the app's own storage, where the launcher and the game read them; the Files app
 * copies, adds, renames and deletes them through here. These folders (Tools sets HOME and XDG_DATA_HOME to them):
 *
 *   Saves and settings  files/share/openrac           the memory cards and the game's settings
 *   Mod files           files/share/openrac/memcard/mod-data  each mod's own files (the SDK's data_dir: a seed, a
 *                       spoiler log). Beside the cards, as this fork's options.cpp keeps them (OpenRAC's own has
 *                       share/openrac/mod-data): where 0.1.17 kept them, so a seed made then is still found.
 *   Games               files/games                   the game's set-up from its disc, its mods
 *   Disc images         files/discs                   the images picked in the launcher
 *
 * OpenRAC's GPU drivers, Screenshots and Diagnostics recordings are left out: the headset's game makes none of them.
 * A document's id is "root", a folder's key, or "key/path inside it".
 */
public final class FilesProvider extends DocumentsProvider {
    private static final class Place {
        final String key;
        final String title;
        final String path;

        Place(String key, String title, String path) {
            this.key = key;
            this.title = title;
            this.path = path;
        }
    }

    private static final String ROOT = "root";
    static final String MOD_FILES = "share/openrac/memcard/mod-data";
    private static final List<Place> PLACES = Arrays.asList(
        new Place("saves", "Saves and settings", "share/openrac"),
        new Place("mod-data", "Mod files", MOD_FILES),
        new Place("games", "Games", "games"),
        new Place("discs", "Disc images", "discs"));
    private static final String[] ROOT_COLUMNS = {
        Root.COLUMN_ROOT_ID, Root.COLUMN_FLAGS, Root.COLUMN_ICON, Root.COLUMN_TITLE, Root.COLUMN_DOCUMENT_ID,
        Root.COLUMN_AVAILABLE_BYTES,
    };
    private static final String[] DOCUMENT_COLUMNS = {
        Document.COLUMN_DOCUMENT_ID, Document.COLUMN_MIME_TYPE, Document.COLUMN_DISPLAY_NAME,
        Document.COLUMN_LAST_MODIFIED, Document.COLUMN_FLAGS, Document.COLUMN_SIZE,
    };

    static String authority(Context context) {
        return context.getPackageName() + ".files";
    }

    /** The folder of the app's files a place is. */
    private static File folder(Context context, Place place) {
        return new File(context.getFilesDir(), place.path);
    }

    /** The document a file of the app's is, or null when it is in none of its folders. A file in Mod files is also in
     *  Saves and settings: the deepest folder names it. */
    static String documentId(Context context, File file) {
        String path = file.getAbsoluteFile().toPath().normalize().toString();
        List<Place> deepest = new ArrayList<>(PLACES);
        deepest.sort((a, b) -> b.path.length() - a.path.length());
        for (Place place : deepest) {
            String base = folder(context, place).getAbsoluteFile().toPath().normalize().toString();
            if (path.equals(base)) {
                return place.key;
            }
            if (path.startsWith(base + File.separator)) {
                return place.key + "/" + path.substring(base.length() + 1).replace(File.separatorChar, '/');
            }
        }
        return null;
    }

    /** Opens the Files app at `file` (one of its folders, or one inside them), else at the game's own place; false when
     *  the headset has no Files app that takes it. Public: OpenRAC's GameActivity calls it (dev.openrac.launcher). */
    public static boolean show(Context context, File file) {
        String authority = authority(context);
        String id = file == null ? null : documentId(context, file);
        int flags = Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_GRANT_READ_URI_PERMISSION
            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION;
        List<Intent> tries = new ArrayList<>();
        if (id != null) {
            tries.add(new Intent(Intent.ACTION_VIEW).setDataAndType(DocumentsContract.buildDocumentUri(authority, id),
                Document.MIME_TYPE_DIR));
        }
        Uri root = DocumentsContract.buildRootUri(authority, ROOT);
        tries.add(new Intent(Intent.ACTION_VIEW).setDataAndType(root, Root.MIME_TYPE_ITEM));
        tries.add(new Intent(Intent.ACTION_VIEW, root));
        tries.add(new Intent("android.provider.action.BROWSE", root));
        // A Files app that answers none of these: the system's Files app itself.
        for (String pkg : new String[] {"com.google.android.documentsui", "com.android.documentsui"}) {
            tries.add(new Intent(Intent.ACTION_MAIN).setClassName(pkg, "com.android.documentsui.files.FilesActivity"));
        }
        for (Intent intent : tries) {
            try {
                context.startActivity(intent.addCategory(Intent.CATEGORY_DEFAULT).addFlags(flags));
                return true;
            } catch (ActivityNotFoundException | SecurityException e) {
                // the next
            }
        }
        return false;
    }

    private Context files() {
        Context context = getContext();
        if (context == null) {
            throw new IllegalStateException("no context");
        }
        return context;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor queryRoots(String[] projection) {
        MatrixCursor cursor = new MatrixCursor(projection != null ? projection : ROOT_COLUMNS);
        MatrixCursor.RowBuilder row = cursor.newRow();
        row.add(Root.COLUMN_ROOT_ID, ROOT);
        row.add(Root.COLUMN_DOCUMENT_ID, ROOT);
        row.add(Root.COLUMN_TITLE, files().getString(R.string.app_name));
        row.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
        row.add(Root.COLUMN_FLAGS, Root.FLAG_LOCAL_ONLY | Root.FLAG_SUPPORTS_CREATE | Root.FLAG_SUPPORTS_IS_CHILD);
        // What the Files app shows under the name: "12 GB free".
        row.add(Root.COLUMN_AVAILABLE_BYTES, new StatFs(files().getFilesDir().getPath()).getAvailableBytes());
        return cursor;
    }

    @Override
    public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection != null ? projection : DOCUMENT_COLUMNS);
        Place place = place(documentId);
        if (place == null) {
            addRoot(cursor);
        } else {
            addFile(cursor, file(documentId), documentId, documentId.equals(place.key) ? place.title : null);
        }
        return cursor;
    }

    @Override
    public Cursor queryChildDocuments(String parentId, String[] projection, String sortOrder)
        throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection != null ? projection : DOCUMENT_COLUMNS);
        if (place(parentId) == null) {
            for (Place place : PLACES) {
                File dir = folder(files(), place);
                dir.mkdirs();
                addFile(cursor, dir, place.key, place.title);
            }
        } else {
            File[] children = file(parentId).listFiles();
            if (children != null) {
                Arrays.sort(children, (a, b) -> a.getName().toLowerCase(Locale.ROOT)
                    .compareTo(b.getName().toLowerCase(Locale.ROOT)));
                for (File child : children) {
                    addFile(cursor, child, parentId + "/" + child.getName(), null);
                }
            }
        }
        cursor.setNotificationUri(files().getContentResolver(), childrenUri(parentId));
        return cursor;
    }

    @Override
    public ParcelFileDescriptor openDocument(String documentId, String mode, CancellationSignal signal)
        throws FileNotFoundException {
        File file = file(documentId);
        if (file.isDirectory()) {
            throw new FileNotFoundException(documentId + " is a folder");
        }
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.parseMode(mode));
    }

    @Override
    public String createDocument(String parentId, String mimeType, String displayName) throws FileNotFoundException {
        if (place(parentId) == null) {
            throw new UnsupportedOperationException("the game's place holds only its folders");
        }
        String name = plainName(displayName);
        File parent = file(parentId);
        File made = new File(parent, name);
        int n = 1;
        while (made.exists()) {
            int dot = name.lastIndexOf('.');
            if (dot <= 0 || Document.MIME_TYPE_DIR.equals(mimeType)) {
                dot = name.length();
            }
            made = new File(parent, name.substring(0, dot) + " (" + n + ")" + name.substring(dot));
            n++;
        }
        boolean ok;
        try {
            ok = Document.MIME_TYPE_DIR.equals(mimeType) ? made.mkdir() : made.createNewFile();
        } catch (java.io.IOException e) {
            ok = false;
        }
        if (!ok) {
            throw new FileNotFoundException("cannot create " + name);
        }
        changed(parentId);
        return parentId + "/" + made.getName();
    }

    @Override
    public void deleteDocument(String documentId) throws FileNotFoundException {
        if (!documentId.contains("/")) {
            throw new UnsupportedOperationException("its folders stay");
        }
        if (!deleteRecursively(file(documentId))) {
            throw new FileNotFoundException("cannot delete " + documentId);
        }
        changed(documentId.substring(0, documentId.lastIndexOf('/')));
    }

    @Override
    public String renameDocument(String documentId, String displayName) throws FileNotFoundException {
        if (!documentId.contains("/")) {
            throw new UnsupportedOperationException("its folders keep their names");
        }
        File file = file(documentId);
        File renamed = new File(file.getParentFile(), plainName(displayName));
        if (renamed.exists() || !file.renameTo(renamed)) {
            throw new FileNotFoundException("cannot rename " + documentId);
        }
        String parentId = documentId.substring(0, documentId.lastIndexOf('/'));
        changed(parentId);
        return parentId + "/" + renamed.getName();
    }

    @Override
    public boolean isChildDocument(String parentId, String documentId) {
        return parentId.equals(ROOT) || documentId.startsWith(parentId + "/");
    }

    /** The place a document is in: null for the root itself. */
    private static Place place(String documentId) throws FileNotFoundException {
        if (documentId.equals(ROOT)) {
            return null;
        }
        int slash = documentId.indexOf('/');
        String key = slash < 0 ? documentId : documentId.substring(0, slash);
        for (Place place : PLACES) {
            if (place.key.equals(key)) {
                return place;
            }
        }
        throw new FileNotFoundException("no document " + documentId);
    }

    /** The file a document is: its place's folder and the path inside it, with no step out of it. */
    private File file(String documentId) throws FileNotFoundException {
        Place place = place(documentId);
        if (place == null) {
            throw new FileNotFoundException("the root is no file");
        }
        int slash = documentId.indexOf('/');
        File file = folder(files(), place);
        if (slash >= 0) {
            for (String part : documentId.substring(slash + 1).split("/", -1)) {
                if (part.isEmpty() || part.equals(".") || part.equals("..")) {
                    throw new FileNotFoundException("no document " + documentId);
                }
                file = new File(file, part);
            }
        }
        if (!file.exists()) {
            throw new FileNotFoundException("no document " + documentId);
        }
        return file;
    }

    private static String plainName(String name) {
        String plain = name.replace('/', '_').trim();
        if (plain.isEmpty() || plain.equals(".") || plain.equals("..")) {
            throw new IllegalArgumentException("no name");
        }
        return plain;
    }

    private void addRoot(MatrixCursor cursor) {
        MatrixCursor.RowBuilder row = cursor.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, ROOT);
        row.add(Document.COLUMN_MIME_TYPE, Document.MIME_TYPE_DIR);
        row.add(Document.COLUMN_DISPLAY_NAME, files().getString(R.string.app_name));
        row.add(Document.COLUMN_FLAGS, 0);
        row.add(Document.COLUMN_SIZE, null);
    }

    private static void addFile(MatrixCursor cursor, File file, String id, String title) {
        boolean dir = file.isDirectory();
        int flags = 0;
        if (file.canWrite()) {
            flags = dir ? Document.FLAG_DIR_SUPPORTS_CREATE : Document.FLAG_SUPPORTS_WRITE;
            if (title == null) {
                flags |= Document.FLAG_SUPPORTS_DELETE | Document.FLAG_SUPPORTS_RENAME;
            }
        }
        String mime = Document.MIME_TYPE_DIR;
        if (!dir) {
            String name = file.getName();
            int dot = name.lastIndexOf('.');
            String extension = dot < 0 ? "" : name.substring(dot + 1).toLowerCase(Locale.ROOT);
            mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            if (mime == null) {
                mime = "application/octet-stream";
            }
        }
        MatrixCursor.RowBuilder row = cursor.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, id);
        row.add(Document.COLUMN_MIME_TYPE, mime);
        row.add(Document.COLUMN_DISPLAY_NAME, title != null ? title : file.getName());
        row.add(Document.COLUMN_LAST_MODIFIED, file.lastModified());
        row.add(Document.COLUMN_FLAGS, flags);
        row.add(Document.COLUMN_SIZE, dir ? null : file.length());
    }

    /** Deletes `file`, a folder with all in it; a link is removed, not followed. */
    private static boolean deleteRecursively(File file) {
        if (file.isDirectory() && !java.nio.file.Files.isSymbolicLink(file.toPath())) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        return file.delete() || !file.exists();
    }

    private Uri childrenUri(String parentId) {
        return DocumentsContract.buildChildDocumentsUri(authority(files()), parentId);
    }

    /** Tells the Files app to list `parentId` again. */
    private void changed(String parentId) {
        files().getContentResolver().notifyChange(childrenUri(parentId), null);
    }
}
