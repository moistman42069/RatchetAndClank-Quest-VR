# Credits and licences

This port stands entirely on other people's open-source work. Credits name code foundations, design
references and related projects; they do not imply endorsement. The launcher shows the same credits
(`CREDITS.txt`) and every licence (`THIRD_PARTY_NOTICES.txt` with OpenRAC's own notices).

| Project / people | Contribution or relationship |
| --- | --- |
| Insomniac Games; Sony Interactive Entertainment | The original Ratchet & Clank (2002): its game, art, audio and story. Ratchet & Clank is a trademark of Sony Interactive Entertainment. The disc is supplied by the player; nothing of it is in this repository or its releases. |
| [OpenRAC](https://github.com/OpenRAC/OpenRAC) ([openrac.dev](https://openrac.dev)) and its contributors | The decompilation and native port this app runs: the game, its renderer, its Android support, the mod SDK with the views interface VR is built on, and the tools that read the disc and prepare the levels. Used at a pinned commit with the changes in `patches/`. |
| Ratchet & Clank PAL decompilation (rac1-decomp): Kryštof "Lynder063" Malinda, creator and maintainer; Flavius (Veradictus) | The PAL (SCES-50916) decompilation in OpenRAC. |
| Ratchet & Clank NTSC-U decompilation (Lombyte): Mateusz Kłysz, creator and maintainer; Louis-Philippe Le Sieur, Pezza, Kryštof "Lynder063" Malinda, Purab, mechaivan | The NTSC-U (SCUS-97199) decompilation in OpenRAC. |
| [npiriou / Rerac-vr](https://github.com/npiriou/Rerac-vr) | The PC VR mod for ReRAC: the controller layout, eye height, comfort choices and the design of the VR gameplay here, and its notes on the hero's attached items (which of them are Clank and his pack). ISC License. |
| [ReRAC](https://github.com/re-rac/rerac) and its contributors | The Rust rewrite of Ratchet & Clank that Rerac-vr builds on. |
| [OpenRAC / rac1-mods](https://github.com/OpenRAC/rac1-mods) | lock-strafe's hooks and strafe decision, which make Ratchet face where you aim (`vr/src/strafe.h`); keyboard-mouse's notes on the game's pad report. GPL-3.0-or-later. |
| [moistman42069 / Halo CE Decomp Remix VR](https://github.com/moistman42069/HaloCE-DecompRemix-VR-PC-Android), and [astromaddie's HaloCE-VR](https://github.com/astromaddie/HaloCE-VR) before it | Quest know-how proven on the Quest 3: the OpenXR session's sequence, performance levels and refresh fallback, the launcher's patterns, its logs and its updater. |
| OpenRAC's own credits | ReRAC, Wrench, Replanetizer, OpenGOAL's jak-project, splat, spimdisasm, objdiff, asm-differ, decomp-permuter, m2c, wibo, Himuro, the GNU toolchain, newlib, and the PS2 reverse-engineering and decompilation communities. |
| [moistman42069](https://github.com/moistman42069) and this project's development and testing | The Quest port: the VR mod, the frame-path changes to OpenRAC, the launcher, packaging, and device testing. |
| Players and bug reporters | Logs and reports from headsets. Private logs and identifying details are not published. |

## Bundled software

- **OpenRAC** (GPL-3.0-or-later): the game's libraries, its Python tools and their bundled dependencies, listed in OpenRAC's `THIRD_PARTY_NOTICES.md`.
- **Khronos OpenXR loader for Android** 1.1.65: Apache-2.0 OR MIT.
- **CPython for Android** (Chaquopy's package 3.13.16-0): Python Software Foundation License; the package MIT.
- **SDL 3**: zlib License.
- **Dear ImGui, cgltf, stb, Luau, glslang, Volk, Vulkan Memory Allocator** and the others OpenRAC lists: their own licences, in the launcher's licences page.

This project is under the GNU General Public License 3.0 or later ([LICENSE](LICENSE)). It grants no rights in Ratchet & Clank.
