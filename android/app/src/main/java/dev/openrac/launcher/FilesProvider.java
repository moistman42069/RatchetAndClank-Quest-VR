// SPDX-License-Identifier: GPL-3.0-or-later
package dev.openrac.launcher;

import android.content.Context;
import java.io.File;

/**
 * OpenRAC's GameActivity opens the Files app through its launcher's FilesProvider (Kotlin, in
 * launcher/src-tauri/gen/android, which this app does not build): FilesProvider.Companion.show.
 * Here the Quest app's own provider (dev.ratchetquest.vr.FilesProvider, the same places) does it.
 */
public final class FilesProvider {
    public static final Companion Companion = new Companion();

    private FilesProvider() {
    }

    public static final class Companion {
        private Companion() {
        }

        /** Opens the Files app at `file`, else at the game's own place; false when no Files app takes it. */
        public boolean show(Context context, File file) {
            return dev.ratchetquest.vr.FilesProvider.show(context, file);
        }
    }
}
