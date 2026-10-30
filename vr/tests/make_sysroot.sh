#!/usr/bin/env bash
# Runs in WSL: an Android sysroot for qemu-aarch64, from the emulator image's system partition.
set -u
S=/mnt/c/RACQuest/b/sysroot
cd "$S"
D=/usr/sbin/debugfs
mkdir -p root/system/lib64 root/system/bin root/apex/com.android.runtime tmp
$D -R "dump /system/apex/com.android.runtime.capex tmp/runtime.capex" system.ext4 2>/dev/null
$D -R "dump /system/apex/com.android.runtime.apex tmp/runtime.apex" system.ext4 2>/dev/null
ls -la tmp | cut -c25-
cd tmp
if [ -s runtime.capex ]; then
    python3 -c "import zipfile; zipfile.ZipFile('runtime.capex').extractall('capex')"
    python3 -c "import zipfile; zipfile.ZipFile('capex/original_apex').extractall('apex')"
else
    python3 -c "import zipfile; zipfile.ZipFile('runtime.apex').extractall('apex')"
fi
ls apex | head
$D -R "ls -p /lib64/bionic" apex/apex_payload.img 2>/dev/null | head -12 | cut -c1-100
for f in libc.so libdl.so libm.so libdl_android.so; do
    $D -R "dump /lib64/bionic/$f ../root/apex/com.android.runtime/$f" apex/apex_payload.img 2>/dev/null
done
$D -R "dump /bin/linker64 ../root/system/bin/linker64" apex/apex_payload.img 2>/dev/null
cd ..
for f in liblog.so libz.so libandroid.so libEGL.so libGLESv3.so libGLESv2.so libc++.so libbase.so libcutils.so libutils.so libnativewindow.so libvndksupport.so libsync.so libbinder.so libui.so; do
    $D -R "dump /system/lib64/$f root/system/lib64/$f" system.ext4 2>/dev/null
done
cp root/apex/com.android.runtime/*.so root/system/lib64/
ls -la root/system/bin root/system/lib64 | awk '{print $5, $9}' | tail -24
