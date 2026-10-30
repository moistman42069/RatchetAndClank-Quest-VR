// SPDX-License-Identifier: GPL-3.0-or-later
// After the Halo CE Decomp Remix launcher's UpdateProvider (moistman42069).
package dev.ratchetquest.vr;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Hands Android's package installer the downloaded new version (Updater), read-only: the one file
 * content://dev.ratchetquest.vr.update/update.apk. (The app has no AndroidX, whose FileProvider
 * does the same.)
 */
public final class UpdateProvider extends ContentProvider {
    static final String AUTHORITY = BuildConfig.APPLICATION_ID + ".update";
    static final String DIRECTORY = "update";
    static final String APK = "update.apk";
    static final Uri URI = Uri.parse("content://" + AUTHORITY + "/" + APK);

    static File apk(android.content.Context context) {
        return new File(new File(context.getCacheDir(), DIRECTORY), APK);
    }

    private boolean isApk(Uri uri) {
        return uri != null && "content".equals(uri.getScheme()) && AUTHORITY.equals(uri.getAuthority())
            && ("/" + APK).equals(uri.getPath()) && uri.getQuery() == null && uri.getFragment() == null;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!isApk(uri) || !"r".equals(mode)) {
            throw new FileNotFoundException(String.valueOf(uri));
        }
        return ParcelFileDescriptor.open(apk(getContext()), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        return isApk(uri) ? "application/vnd.android.package-archive" : null;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        if (!isApk(uri)) {
            return null;
        }
        MatrixCursor cursor = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {APK, apk(getContext()).length()});
        return cursor;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
