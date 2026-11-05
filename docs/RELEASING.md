# Releasing

A release is what the launcher's updater installs (`UpdatePolicy.java`), so its shape is fixed:

| | Must be |
| --- | --- |
| Tag | `vX.Y.Z`, the APK's `versionName` with a `v` |
| APK asset | `RatchetQuestVR-X.Y.Z.apk`, exactly (`tools/build_quest.py` names it so) |
| Release | Published, not a draft and not a pre-release: the updater reads GitHub's *latest* release only |
| `versionCode` | Higher than the last release's |
| Signing key | The project's release key, whose certificate is `tools/release-cert.sha256` |

A test build for a few people goes up as a **pre-release**: the updater never offers it.

## The release key

`C:\RACQuest\keys\release.jks` with its passwords in `C:\RACQuest\keys\release.properties`, never
in this repository (point `RACQUEST_SIGNING` at another properties file if they move). **Keep a
copy somewhere safe.** Android installs an update only over an app signed with the same key: with
the key lost, every player would have to uninstall (losing the set-up) to take a new version, and
the updater would refuse it. `build_quest.py --release` stops if the APK's certificate is not the
one in `tools/release-cert.sha256`.

## Steps

1. Raise `versionCode` by one and set `versionName` in `android/app/build.gradle`; the same version in
   `vr/openrac-mod.json`, `OPENRAC_MOD_CPP` at the end of `vr/src/quest_vr.cpp` and the headset report's
   default (`.github/ISSUE_TEMPLATE/headset-report.yml`).
2. Note the release in `docs/STATUS.md`, and in the README's **Latest** line.
3. Build and check everything:
   ```sh
   source env.sh
   python tools/build_quest.py --release          # android/build/RatchetQuestVR-X.Y.Z.apk
   python tools/build_pc.py                        # the game and the mod for the PC test
   OPENRAC_DIRECT=1 bash vr/tests/pc_run.sh t2 --level 1   # the real game, a simulated headset
   ```
   Look at the shots in `C:\RACQuest\pc\out\t2` and the `[vr-…]` lines in its `stdout.txt`.
   The PC test runs the PAL disc only. For NTSC-U, `build_quest.py` runs `tools/check_hooks.py` after
   the port build: it stops if any hook point the mod takes is missing, or has another signature, on
   either disc. Such a disc refuses the whole mod and the headset stays black, as on the US disc
   from 0.1.2 to 0.1.5.
4. Test the APK on a headset: a fresh install and set-up, an update over the last release (its
   set-up and saves kept), Play in VR, the 2D test.
5. Commit, tag and push:
   ```sh
   git tag vX.Y.Z && git push origin main vX.Y.Z
   ```
6. Publish, with the checksums:
   ```sh
   cd android/build && sha256sum RatchetQuestVR-X.Y.Z.apk > SHA256SUMS.txt
   gh release create vX.Y.Z RatchetQuestVR-X.Y.Z.apk SHA256SUMS.txt \
     --title "Ratchet & Clank Quest VR X.Y.Z" --notes-file ../../docs/release-notes.md
   ```
   GitHub records each asset's SHA-256 itself, which the updater checks the download against.

## Release notes

Start from the last release's: what is new, what was fixed, what is still WIP, what was and was
not tested on a headset, how to install and update (do not uninstall: the set-up and saves go with
it), where the logs are (`Download/RatchetQuestVR`), and how to report.

## Updating OpenRAC

1. Write the current changes out first, `git stash` is no backup. A plain diff leaves new files out, so mark each new
   file first with `git -C third_party/OpenRAC add -N <file>`, which is not a commit (`git -C third_party/OpenRAC status
   --short` lists them as `??`). For 0.1.12 and 0.1.13 they are `port/game/rac1-ntsc/host/vector.c`,
   `port/game/rac1/host/hero_anim_guard.c`, `port/game/common/frontend/hero_shadows.h` and
   `port/tests/hero_shadow_rig_test.cpp`. Then
   `git -C third_party/OpenRAC diff --full-index > patches/0001-quest-vr-frame-path.patch`. On Windows, gen_binding.py
   writes CRLF: put its files back to LF first, or the patch carries the line endings.
2. `git -C third_party/OpenRAC fetch origin develop`, check it out, and `git apply` the patch.
3. Read upstream's diff of every patched file, and every new window swap or draw path: in VR the
   headset's frame replaces the window's swap.
4. Set `OPENRAC_PIN` in `tools/build_quest.py`, commit the submodule's new commit, and note the
   version in the README and `docs/STATUS.md`.
5. If `editor/port.py`'s `FORMAT` changed, the update rebuilds the players' levels: the launcher
   offers it on the Play tab.
