# Changelog

All notable changes to this project are documented in this file.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**Rule** — every tagged release (`vX.Y.Z`) must have a corresponding entry
here BEFORE the tag is pushed. Pull requests that bump the version without
a `CHANGELOG.md` entry are rejected. See `CONTRIBUTING.md`.

---

## [Unreleased]

### Added
_nothing yet_

### Changed
_nothing yet_

### Fixed
_nothing yet_

---

## [0.1.1] — 2026-04-19

### Fixed
- **Window icon**: restored the original 1999 `blaster.ico` (converted to
  64×64 PNG). Previous builds shipped a custom TakoHi icon by mistake.
- **Monsters stuck at ceiling**: iter-3 port of `MAIN.ASM:3115
  mov sens_y,-1` was taken literally, which in C's Y-down convention
  clamped monsters at `y=0` with `vy=-1` forever. Reverted to the
  conditional flip `if (vy<0) vy=-vy` that mirrors what
  `detect_colision_wall` (MAIN.ASM:3497-3530) already does in effect.
  The ASM `mov sens_y,-1` is almost certainly a typo for `neg sens_y`.
- **Demo exit on menu click**: a held mouse button from the menu
  immediately exited the demo in C (no ASM-equivalent palette fades to
  debounce). Ported `wait_click_release` / `wait_keyboard_release` from
  `MOUSE.ASM:420-433` + `MAIN.ASM:266-270`; called from
  `screen_menu.c::menu_apply_action` after every menu button dispatch.

---

## [0.1.0] — 2026-04-19

### Added
- Initial public release.
- Full C99 port of **BrickBlaster** (Media Pocket, 1999, Eclipse demoscene
  team) using raylib 5.0 for rendering / audio / input / windowing.
- Windows x64 (MSVC), Linux x64 (GCC/Clang), macOS arm64, Android (NDK)
  supported.
- 80 original levels across 2 worlds (Space, Eclipse), byte-compatible
  with the 1999 `.lv0` / `.lv1` / `.lv2` format.
- 24 power-ups byte-identical to the ASM `struc_options` table
  (multi-ball, laser, magnetic, iron ball, ghost, night mode, teleporter,
  bonus/malus, big/mini-shoot, large/small ship, reverse, +others).
- 4 monster types with per-difficulty spawn cadence (MAIN.ASM:2958).
- 7-screen hierarchical menu, FONTE bitmap font renderer, panel_info
  overlay text routed through i18n (EN/FR/ES byte-exact from
  `Blaster.cfg`; DE/IT/PT best-effort).
- High-score table with original XOR-encoded save format (HISCORE.ASM:454
  `Code_Score`), 15-char names, per-mode separation (solo / coop).
- Demo / attract mode with AI paddle that tracks the ball (MAIN.ASM:5131
  `demo_move_x_player_1`).
- 2-player coop (shared score & lives) and dual/versus (separate) modes.
- Level editor with 5 brick types and `F10` clipboard xchg (EDITOR.ASM).
- Intro + credits + victory FLC animations playing at the original 12 FPS
  cadence (FILE.ASM:98 / 164).
- Compile flags `TAKOHI_BRANDING` and `GIF_RECORDER` (both `ON` by
  default, both can be turned off for a 100%-vanilla build).
- Three iterations of automated ASM-fidelity audits resulting in
  ~40 P0/P1 fixes documented in `audit-findings.md` and
  `audit-asm-faithful.md`.
- GitHub Actions CI building all three desktop platforms + creating
  release artefacts on tag push.

### Licence
- GPL v3 — inherited from upstream david4599/BrickBlaster.

[Unreleased]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.1...HEAD
[0.1.1]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/KonsomeJona/brickblaster-c/releases/tag/v0.1.0
