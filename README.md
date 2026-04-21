# Brick Blaster — C/raylib port

<img src="img/BrickBlaster_banner.png" alt="Brick Blaster Banner">

[![Status: preview](https://img.shields.io/badge/status-preview-orange)](#known-gaps-vs-the-1999-original)
[![Upstream ASM source](https://img.shields.io/badge/upstream-david4599%2FBrickBlaster-blue?logo=github)](https://github.com/david4599/BrickBlaster)
[![EOS Archive](https://img.shields.io/badge/archive-david4599%2FBrickBlaster--EOS--Archive-blue?logo=github)](https://github.com/david4599/BrickBlaster-EOS-Archive)
[![License: GPL v3](https://img.shields.io/badge/license-GPL_v3-blue.svg)](LICENSE)
[![Build](https://github.com/KonsomeJona/brickblaster-c/actions/workflows/build.yml/badge.svg)](https://github.com/KonsomeJona/brickblaster-c/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/KonsomeJona/brickblaster-c)](https://github.com/KonsomeJona/brickblaster-c/releases/latest)

> **Status: early preview, actively iterating.** The ASM-level internals
> (gameplay constants, level format, XOR codec, power-up tables,
> collision LUT) are ported byte-for-byte with ASM line citations. The
> UI polish pass (menu layout, intro timing, a few power-up animations)
> is still in flight — see the [polish checklist](#polish-checklist-vs-the-1999-original)
> for what's being worked on next.

> **Based on the original x86 assembly sources** of BrickBlaster,
> released for MS-DOS on *Media Pocket 1999* by the **Eclipse** demomaker
> team and open-sourced under GPL v3 in 2024.
> Upstream repo: **[david4599/BrickBlaster](https://github.com/david4599/BrickBlaster)** — contains the original
> `MAIN.ASM`, `HISCORE.ASM`, `FONTE.ASM`, `EDITOR.ASM`, `FILE.ASM`,
> `DRAW.ASM`, `MOUSE.ASM`, `Blaster.inc`, `Blaster.cfg` (FR / EN / ES)
> and the Watcom/DirectX toolchain to rebuild the 1999 binary.
>
> Companion archive: **[david4599/BrickBlaster-EOS-Archive](https://github.com/david4599/BrickBlaster-EOS-Archive)** —
> the unmodified archival snapshot: original sources and 1999 binaries,
> the **WinEOS** 4.00 Alpha runtime, and the standalone **EOS** 3.05 /
> 3.06 DOS extenders (EOS = *Eclipse Operating System*, the custom
> DOS extender the Eclipse demomaker team built for their productions,
> later adapted to Windows/DirectX as WinEOS to host BrickBlaster).
>
> This repo is a **C/raylib translation** of those sources, byte-exact
> for all gameplay constants and file formats. Every non-trivial
> function cites its ASM line in a comment.

This port uses [raylib 5.0](https://www.raylib.com/) for rendering,
audio, and input, and runs on Windows, macOS, Linux, and Android.

## What is BrickBlaster?

A polished Arkanoid-style brick breaker with:
- **80 levels** across 2 worlds (Space, Arcade)
- **24 power-ups** (multi-ball, iron ball, laser, magnetic paddle, ghost, teleporters, bonus/malus, …)
- **Monsters** that spawn periodically and must be dodged or destroyed
- **2-player** coop and versus (duel) modes
- **Level editor** to design your own levels
- **Demo / attract mode** with AI paddle
- UI localized in English, French, German, Spanish, Italian, Portuguese
  (in-game power-up labels stay in the original game config — FR / EN / ES)

## Fidelity goal

The **target** is byte-exact parity with the 1999 x86 binary. The
current state of play:

**Ported byte-for-byte from the ASM** (stable, cross-checked against
`MAIN.ASM` / `HISCORE.ASM` / `FONTE.ASM` / `EDITOR.ASM` / `FILE.ASM` /
`DRAW.ASM` / `MOUSE.ASM` / `Blaster.inc` / `Blaster*.cfg`):
- Physics & collision LUT, ball/angle/speed bookkeeping
- Scoring, per-difficulty spawn frequencies, `Time_Between_Option`
- 24-entry `struc_options` power-up table + per-difficulty frequency triplets
- High-score XOR codec and `blaster.scr` on-disk layout
- Level file format (`.lv0` / `.lv1` / `.lv2`, 390 bytes × 80 levels)
- Sprite tile offsets and per-world palette selection
- Timing constants (`DELAI_OPTION`, `DELAI_DEMO`, bonus life threshold,
  etc.)

**In-flight polish pass** (see [checklist](#polish-checklist-vs-the-1999-original)):
menu layout, intro timing, a few power-up animations, iron-ball /
unbreakable-brick interaction, ESC-to-menu on desktop, teleport
animation.

Per-iteration audit trail with ASM line citations:
[audit-findings.md](audit-findings.md) and
[audit-asm-faithful.md](audit-asm-faithful.md).

## Polish checklist vs the 1999 original

Items on the next-iterations list, mostly UI and a few gameplay
edges. Contributions and bug reports from people who know the 1999
binary are genuinely useful — open an issue or a PR.

**Done (shipped in post-v0.1.4 commits):**
- ✅ **Iron ball**: passes through indestructible bricks and renders
  blue while active — matches `MAIN.ASM:2844-2849` + `3862-3864`
  (`sprite_rebond = Off` disables the bounce, sprite swap is per-frame
  in the ball loop). Dual-mode colour swap mirrored.
- ✅ **Teleport power-up**: teleports each ball to a random free
  position inside the play area (skips incassable tiles), mirroring
  `MAIN.ASM:1526-1563 Teleporte_Ball`. Same routine is now used by
  the `BRICK_TELEPORTER` hit path (was also wrong).
- ✅ **Power-up hitbox**: 26×24 (was inflated to 52×48 — leftover from
  the Wear OS 2x draw path). ASM-exact containment check.
- ✅ **Unbreakable bricks flicker**: removed the ambient frame-toggle
  that pulsed every incassable brick in sync. Hit-triggered reflet
  animation per-brick still on the list for later.
- ✅ **ESC binding**: ESC toggles pause from gameplay and exits to the
  main menu from the pause screen (desktop UX; `P` / click / gamepad
  still resume as before).
- ✅ **Startup intro**: plays only `media.png` (Media Pocket splash,
  `MAIN.ASM:54 File_Editor = Media.gif`) + `credit_b.png`. The
  animated `intro.flc` logo and the remaining credit letters are part
  of the **Credits** menu (`MAIN.ASM:146-190 @@credit`), not launch.
- ✅ **Android doc**: `android/README.md` added (honest scaffold
  status).

**Still on the list:**
- **Menu**: pin the `Brick Blaster` logo at the top; swap the square
  selection highlight for a rounded shape matching the logo; make
  selection text follow the mouse; wire up the sound settings screen;
  fix the cropped `MAIN MENU` header.
- **Ball counter glitch**: players sometimes receive `3 balles`
  mid-game with no powerup pickup — root-cause and fix (needs repro).
- **Power-up animations**: paddle explosion / telepod / shoot
  sequence wiring (`sprite_nbs_shape` animation system not yet
  ported for paddle & powerups — only monsters animate today).
- **Unbreakable brick reflet animation**: per-brick hit-triggered
  ripple (`MAIN.ASM:4058-4061 + 6220-6240`), replacing the removed
  ambient flicker.
- **Credits menu**: prepend the `intro.flc` 36-frame animation before
  the remaining credit letters (M / G / C / W / E), matching the
  `@@credit` loop.

## Build

### Prerequisites
- CMake ≥ 3.16
- C99 compiler (MSVC / GCC / Clang)
- Internet access on first configure (raylib 5.0 is fetched via `FetchContent`)

### Windows (Visual Studio 2022)
```bash
cmake -G "Visual Studio 17 2022" -A x64 -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --config Release
build\Release\brickblaster.exe
```

### Linux / macOS
```bash
cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build
./build/brickblaster
```

### Android
See `android/README.md` (Gradle + NDK).

### Build flags

| Flag | Default | Effect |
|---|---|---|
| `-DTAKOHI_BRANDING=OFF` | `ON` | Strip TakoHi publisher splash at end of intro |
| `-DGIF_RECORDER=OFF` | `ON` | Remove F12 GIF capture (reduces binary) |

For a 100%-vanilla build matching the 1999 release exactly:
```bash
cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DTAKOHI_BRANDING=OFF -DGIF_RECORDER=OFF
```

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move paddle | ← → / mouse | Left stick / D-pad |
| Fire ball | Space | A (Cross) |
| Pause | P / Esc | Start |
| Back | Esc | B (Circle) |
| Screenshot GIF | F12 | — |

**2-player keyboard** — P1 arrows + Space, P2 Q/A/D + F.
**2-player gamepad** — P1 on gamepad 0, P2 on gamepad 1.

## Project structure

```
src/              Game source (C99, raylib)
assets/
  sprites/        Sprite atlases (PNG)
  audio/          Sound effects (WAV, ex-IFF)
  music/          Music tracks (WAV, ex-MOD)
  intro/          Intro animation frames (PNG)
  final/          Victory animation frames
  credits/        Credit slides
  levels/         Level files (.lv0 / .lv1 / .lv2, 390 bytes × 80 levels)
  backgrounds/    Menu / hiscore backgrounds
data/
  blaster.cfg     Optional runtime config override (mirrors ASM Blaster.cfg)
  blaster.scr     High scores (persistent, XOR-encoded)
  blaster.usr     Volume settings (2 bytes, per FILE.ASM:813)
third_party/
  gif.h           Public-domain header-only GIF encoder (for F12 recorder)
audit-findings.md       Iter 3 ASM fidelity audit report
audit-asm-faithful.md   Iter 1/2 audit archive
```

## Modifications from the 1999 original (GPL §5 notice)

This work is a modified version of **BrickBlaster** (1999). Per GPL v3
section 5, the following prominent modifications were made in 2026:

- Full C99 rewrite of the x86 assembly source using raylib 5.0 for
  rendering, audio, and input abstraction.
- Target platforms extended from Windows/DOS to Linux, macOS, Android.
- Main loop converted from vsync-locked 70 Hz (DOS IRQ timer) to 60 Hz
  via raylib frame pacing with custom `SUPPORT_CUSTOM_FRAME_CONTROL`.
- Palette-indexed VGA rendering replaced by RGBA textures. The palette
  fades in `DRAW.ASM` (Shade_On/Shade_Off) are approximated with alpha
  overlays. Night-mode palette swap replaced by an RGBA dim overlay.
- Audio pipeline: IFF samples converted to WAV; MOD tracks converted to
  looped WAV streams played via raylib's `MusicStream`.
- File I/O: level/high-score/config files kept byte-compatible with
  the 1999 format; launcher paths adapted to modern OS conventions.
- Optional compile-time additions (both OFF for vanilla builds):
  - `TAKOHI_BRANDING` — publisher splash slide at end of intro.
  - `GIF_RECORDER` — F12 in-game GIF capture for demo recording.

No gameplay constant, scoring rule, power-up table, level layout, or
string is intentionally altered — the 24 power-ups match `struc_options`
byte-for-byte and the 80 levels load unchanged from `.lv0` / `.lv1` /
`.lv2`. A few visible behaviours still diverge while the polish pass
is in progress (see [checklist](#polish-checklist-vs-the-1999-original));
deliberate deviations are tracked in
[audit-findings.md](audit-findings.md).

## Credits

**Original 1999 game (Eclipse demomaker team):**
- **Marc Radermacher** ("Hacker Croll") — code (BrickBlaster + WinEOS)
- **Christophe Résigné** ("Rez") — music
- **Frédéric Box** ("Profil") — graphics
- **Régis Vidal** ("Light Show") — code (WinEOS)

Developed by [Carapace (Softplace)](https://www.abandonware-france.org/compagnies/carapace-82/),
published by [Media Pocket](https://www.abandonware-france.org/compagnies/media-pocket-1019/).

**Source preservation & upstream archive (2024):**
In early 2024, **Marc Radermacher** entrusted the original source tree
to [david4599](https://github.com/david4599), who published it on GitHub
as two repositories:
- [BrickBlaster](https://github.com/david4599/BrickBlaster) — the
  working source tree with a ready-to-use `build.bat` (Watcom 11.0B +
  DirectX 6 SDK) and documented patches (`FILE.ASM` env-var limit and
  `MAIN.ASM` privileged-instruction fix) so the 1999 binary rebuilds
  cleanly on Windows 95 through 11.
- [BrickBlaster-EOS-Archive](https://github.com/david4599/BrickBlaster-EOS-Archive)
  — the unmodified archival snapshot: original sources and 1999
  binaries, WinEOS 4.00 Alpha, and the standalone EOS 3.05 / 3.06
  DOS extenders.

**C/raylib port (2026):** [Jonathan Odul (konsomejona)](https://github.com/konsomejona).

**Third-party:**
- [raylib](https://www.raylib.com/) 5.0 (zlib license)
- [gif-h](https://github.com/charlietangora/gif-h) — public domain

A huge thanks to **Marc Radermacher** ([edromel.com](https://www.edromel.com))
for kindly allowing the original sources to be preserved and to
**david4599** for the archive that made this port possible.

## License

**GNU General Public License v3.0** — inherited from the original
BrickBlaster sources. See [LICENSE](LICENSE).

This program comes with **ABSOLUTELY NO WARRANTY**. This is free
software, and you are welcome to redistribute it under the conditions
of the GPL v3.

## Release notes

Every tagged release has a corresponding entry in [CHANGELOG.md](CHANGELOG.md).

**Release rule (enforced):** a PR that bumps the version or pushes a
`vX.Y.Z` tag MUST add/update the matching `CHANGELOG.md` section BEFORE
the tag is pushed. No changelog entry = no release. The workflow
`.github/workflows/build.yml` ships the changelog alongside each
binary artefact, and GitHub release notes are generated from it.

## Links

- Upstream source + toolchain: [david4599/BrickBlaster](https://github.com/david4599/BrickBlaster)
- Unmodified original + EOS archive: [david4599/BrickBlaster-EOS-Archive](https://github.com/david4599/BrickBlaster-EOS-Archive)
- French commercial release (1999): [archive.org](https://archive.org/details/brick-blaster-1999)
- Spanish commercial release: [archive.org](https://archive.org/details/brick-blaster-1999-spanish)
- Eclipse demomaker team: [eclipse-game.com](https://www.eclipse-game.com)
