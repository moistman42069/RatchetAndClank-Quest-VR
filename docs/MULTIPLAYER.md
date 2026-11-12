# Multiplayer in VR: what is ours, and keeping it working

The headset bundles OpenRAC's Multiplayer mod (github.com/OpenRAC/openrac-mods, `multiplayer/`). Everything VR
players see of each other in a party (heads, hands, fingers, held items, Swingshot catches of the host's enemies,
bolt turns) rides on a few small additions. This page lists each one, the formats they carry, which games work
with which, and what to do when upstream Multiplayer moves.

The rule behind all of it: **a party with a plain (flat) game in it still plays.** A VR game that does not hear
from a VR host plays as a flat player's member would, and a flat game never sees anything it does not know.

## The pieces

| Where | What | Why |
|---|---|---|
| `patches/rac1-mods/0001-multiplayer-0.12.2.patch` | Multiplayer's version back to 0.12.2 (manifest, HELLO, `OPENRAC_MOD_CPP`, README); keeps 0.12.3's one change, the verbose log (`mp-trace.log`) in the mod's data folder | A party needs the same Multiplayer version in every game. The pinned openrac-mods commit says 0.12.3 (unreleased); OpenRAC's mod list, which PC players get, gives 0.12.2. On a Quest the working folder is `/`, where the log could not open |
| `patches/rac1-mods/0002-player-extra.patch` | A DATA kind for each player's word on itself, `kind::kPlayerExtra` = 84; a member sends its word with every other pose, the host puts its own and its members' after the poses in its POSES datagrams | The VR mod's word (below) reaches the other games. Kind 84 is unowned upstream, so a plain game drops it unread |
| `patches/0001-quest-vr-frame-path.patch` (the port, `third_party/OpenRAC`) | `party_role`, `moby_hit_as_game`, `player_extra_publish/read/deliver`, `on_player_extra`, `figure_arms`, `figure_hand_moby` (the "Quest fork's" entries at the end of `openrac_api`, `sdk/include/openrac/openrac_mod.h`); `on_moby_hit` taking several mods (`port/game/common/mod_play.cpp`, `tell_hit_watchers`) | The game-side half: which a game is to the party, a member's wrench and fist hits going to the host, passing words between mods, drawing a figure's VR arms and held item. Several hit watchers so RAC-Board and Multiplayer load together (each takes hits on different mobys) |
| `vr/src/quest_vr.cpp` | The VR mod's side, below | |

`tools/build_quest.py` checks each checkout is at its pin (`RAC1_MODS_PIN`, `RAC_BOARD_PIN`) and applies the patches
(`apply_patches`: skipped when already applied, the build stops when one neither applies nor is applied). The PC
build (`tools/build_pc.py`) builds the same patched checkouts. Each patch file starts with a note on what it does
and how to remake it.

## The VR mod's side (`vr/src/quest_vr.cpp`)

- `party_role_now`, `party_tick`, `party_role_told`: which this game is (the port's `party_role`: 0 none, 1 host,
  2 member). A host is a game whose rules its Multiplayer read in the last 2 s; after a stall or a film that long
  it reads as a member until Multiplayer's next frame, so `party_tick` takes a host/member change only once it
  holds half a second. `QUEST_PARTY_ROLE` forces a role in PC tests.
- `party_rules_came`, `kRuleVersion`: the host's rules (below), which say what the host's game does for members.
- `publish_own_word`, `extra_decode`, `player_extra_came`, `pose_remote`, `ease_word`: this player's word out, the
  others' in and drawn on their figures.
- `grab_through_host`, `party_grab_host_tick`: a member's Swingshot catch, driven by the host.
- `party_crank_host_tick`, `party_bolt_crank`, `party_bolt_finished`: a member's bolt turn; the host turns its own
  copy (what a bolt opens is the host's), and finishes it with its hero lent to the bolt for one tick.
- `self_slow_capture`, `party_all_wheels`: the weapon wheel slows only its own player in a party unless every
  player has it up.

## Formats

Both start with `'Q' 'V'` and a version byte, then a flags byte. Readers check the version with `>=` and ignore
flags and bytes they do not know, so a newer game always reads an older one and an older one reads as much of a
newer one as it knows.

### The rules (`party_rules_publish`, at most a few bytes)

`'Q' 'V' version cheats`. Cheats: invincible 1, ammo 2, bolts 4 (a member keeps a cheat on only if the host's game
has it on too).

| Version | From | The host's game does for its members |
|---|---|---|
| 1 | 0.1.18 | Cheats follow the host |
| 2 | 0.1.19 | Drives members' Swingshot catches; players' heads and hands shown |
| 3 | 0.1.23 | Turns its bolts with a member's turn; fingers, world scale and body stretch in the word |

### The word (`player_extra_publish`, at most 200 bytes, `OPENRAC_PLAYER_EXTRA_MAX`)

`'Q' 'V' 1 flags`, then a block for each flag set, in this order (positions in the hero's own frame: his feet and
heading, so the word rides on his figure wherever the others see it):

| Flag | Block | Bytes | From |
|---|---|---|---|
| Head 1 | neck (3 × i16, 1/2048 unit), gaze/left/crown rows (9 × i16, 1/16384), then per hand: bits (shown 1, free 2), grip u8, palm rows (9 × i16, 1/8192), palm place (3 × i16) | 76 | 0.1.19 |
| Item 2 | the held item's rows (9 × i16), place (3 × i16), scale u8 (×64) | 25 | 0.1.19 |
| Wheel 4 | (none: the weapon wheel is up) | 0 | 0.1.19 |
| Grab 8 | a member's catch: moby key i32, place 3 × f32, velocity 3 × f32 | 28 | 0.1.19 |
| Shape 16 | world scale u8 (×64), body stretch u8 (cm), body height i8 (cm); per hand: finger mode u8 (game 0, grip 1, tracked 2, fist 3, lift 4), first/rest/thumb/first lift/thumb lift u8 (/255), 5 finger offsets i8 (/100) | 25 | 0.1.23 |
| Crank 32 | a member's bolt: moby key i32, progress u16 (/65535; 1.0 for 1.5 s after it finishes) | 6 | 0.1.23 |

A full word is about 165 bytes. The host fits words into the room its POSES datagrams leave (1200 bytes each), so
four players fit in one datagram; with more, a word that finds no room goes with the next poses.

**Adding something:** give it the next flag (64, then 128; after that, a second flags byte behind a new flag) and
append its block after the last one. Never change or reorder an existing block. If a host must act on it, bump
`kRuleVersion` and test `bytes[2] >= N` in `party_rules_came`, so a member knows whether the host will.

## Which games work with which

| Host | Member | What the member gets |
|---|---|---|
| Quest 0.1.23 | Quest 0.1.23 | Everything: bodies with fingers, Swingshot catches, bolt turns, the party wheel, the host's cheats |
| Quest 0.1.23 | Quest 0.1.19 to 0.1.22 | Bodies (no fingers, its own height cap), catches, cheats. The newer host reads the older word |
| Quest 0.1.19 to 0.1.22 | Quest 0.1.23 | Bodies, catches, cheats; a bolt turned here turns only this game's bolt (the log says so) |
| Quest 0.1.18 or older | any Quest | Not checked: its Multiplayer may not even match |
| Flat (OpenRAC on a PC, no VR mod) | Quest | Wrench, fists and guns as always; no Swingshot catches of enemies; the wheel slows only this player; no cheats. Turned off by itself: the VR mod hears no rules |
| Quest | Flat | The flat player plays as always and sees ordinary figures; the Quest players' words are dropped unread |

Every game needs the same Multiplayer version (0.12.2 now) and the same party digest, or Multiplayer marks the party
"(differs)" in Browse and its host's game does not take the player. The digest is the mods that change the game, by
id and version (RAC-Board, Difficulty, Item randomizer and the like): every player turns on the same ones. The VR
mod's manifest says `"multiplayer": "input"`, so it does not count, and VR and flat games share parties. The Quest's
patched Multiplayer and a PC's plain one share parties too: patch 0002 changes neither the version nor the digest.

## When upstream Multiplayer updates

1. **OpenRAC's mod list moves to a new release.** Move `RAC1_MODS_PIN` to that release's commit. Drop patch 0001
   if the release's version matches what PC players get (otherwise remake it with that version: the command is in
   the patch's note). Check whether the release already keeps the verbose log in the data folder.
2. **Re-apply 0002.** `git -C <openrac-mods> apply --check patches/rac1-mods/0002-player-extra.patch`. It touches
   only the coop files, `game_adapter` and `kinds.hpp`. If it no longer applies, redo its few hunks by hand: a DATA
   kind, the member's send with its pose, the host's relay after its poses, and the delivery to
   `player_extra_deliver`. Then `git diff` to regenerate it, keeping the note at its top.
3. **Upstream takes kind 84.** Change `kind::kPlayerExtra` to a free number. Old Quest builds keep 84 and would not
   hear new ones (they play as with a flat host), so say so in the release notes.
4. **Upstream adds its own per-player channel for mods.** Drop 0002 and point `game_adapter`'s `player_extra` and
   `player_extra_deliver` at it. The VR mod does not change: it only talks to the port.
5. **Upstream changes how hits or the host's world work.** The pieces to check are `moby_hit_as_game` (a member's
   hit going to the host) and Multiplayer's `on_moby_hit` taking hits on figures and driven mobys. If upstream makes
   bolts or other interactions shared, `party_crank_host_tick` can go: delete the Crank flag's use, keep its number
   reserved.
6. **Build and test:** `python tools/build_quest.py --no-port --no-apk` (the mods only), the PC build, then the party
   sims: `wrench-mp`, `handpunch-mp`, `akimbo-mp`, `handpunch-member`, `swinggrab-member`, `wheel-member`,
   `options-mp`, `racboard-mp`, `racboardbomb-mp` (`bash vr/tests/regress.sh <name>`). The sims run one game; the
   words between two games are only tested in a real party, so ask a tester for both players' logs.
