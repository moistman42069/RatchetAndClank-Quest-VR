#!/bin/sh
# The tools' folders through an install and three updates on a Linux filesystem, with the real deleteRecursively from
# Tools.java (Windows cannot make the links, so LauncherTest skips its own check there). An update moves Android's
# library folder, which leaves the links in the last tools dangling. Run it in WSL or on Linux, from anywhere.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
SRC=$HERE/android/app/src/main/java/dev/ratchetquest/vr/Tools.java
W=$(mktemp -d)
cd "$W"
{
  echo 'import java.io.*; import java.nio.file.*;'
  echo 'public class H {'
  sed -n '/static void deleteRecursively(File file) {/,/^    }$/p' "$SRC"
  cat <<'EOF'
    static void link(File target, File link) throws Exception {
        link.getParentFile().mkdirs();
        link.delete();
        Files.createSymbolicLink(link.toPath(), target.toPath());
    }
    static void install(File files, int n) throws Exception {
        File libs = new File(files, "app-" + n + "/lib");
        libs.mkdirs();
        new File(libs, "libopenrac-python.so").createNewFile();
        File root = new File(files, "openrac"), fresh = new File(files, "openrac.new"), old = new File(files, "openrac.old");
        deleteRecursively(fresh);
        deleteRecursively(old);
        new File(fresh, "tools").mkdirs();
        if (root.exists() && !root.renameTo(old)) {
            deleteRecursively(root);
        }
        if (root.exists() || !fresh.renameTo(root)) {
            throw new IOException("cannot put the tools in place");
        }
        deleteRecursively(old);
        link(new File(libs, "libopenrac-python.so"), new File(root, "python/bin/python3"));
        if (Files.exists(old.toPath(), LinkOption.NOFOLLOW_LINKS)) {
            throw new IOException("install " + n + " left openrac.old behind");
        }
    }
    public static void main(String[] a) throws Exception {
        File files = new File("files");
        files.mkdirs();
        for (int n = 1; n <= 4; n++) {
            if (n > 1) {
                deleteRecursively(new File(files, "app-" + (n - 1)));
            }
            install(files, n);
        }
        System.out.println("an install and three updates: nothing left behind");
    }
}
EOF
} > H.java
javac H.java
java H
rm -rf "$W"
