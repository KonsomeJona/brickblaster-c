# Changelog

All notable changes to this project are documented in this file.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

**Rule** — every tagged release (`vX.Y.Z`) should have a corresponding entry
here BEFORE the tag is pushed. Nothing enforces this automatically today:
no CI job checks the changelog, and `build.yml` generates GitHub release
notes from commits (`generate_release_notes: true`), shipping this file
inside the artefact rather than reading from it.

---

## [Unreleased]

Version in `CMakeLists.txt` and `android/app/build.gradle` is already
`0.1.8`; `v0.1.6` and `v0.1.7` were never tagged. Everything below is
unreleased and lives on `fix/asm-parity-2026-08`.

### Added
- **Web (WebAssembly) target** — static Emscripten bundle, `web/shell.html`
  and a `web.yml` CI workflow.
- **Installable Android debug APK** — the Gradle project was a scaffold
  before; plus `smallestScreenSize` / `sensorLandscape` handling for
  tablets and foldables.
- **CI matrix extended to 5 targets** — Linux x64/arm64, macOS universal,
  Windows x64/arm64.
- **Faithful 1999 random generator** (`src/asm_random.c`) — `calc_random`
  and `get_random` ported bit for bit, defects included, verified over
  100 000 draws against an independent x86 emulator. `wait_synchro`'s
  per-frame tick is ported too.
- **Side-wall pillars** — `create_border` (`MAIN.ASM:5825-5852`) stamps
  five 42×96 tiles per side; the port had replaced them with a plain dark
  veil.
- **Active power-up icon in the HUD** — `panel_option` at (122, 446),
  drawn from the `option_fade_o` row (`MAIN.ASM:5688-5694`). The port
  showed a text banner and no icon.

### Changed
- **Demo / attract mode is a real game again** — score, power-up
  collection and power-up effects are no longer suppressed. `demo_flag`
  appears nowhere in `FONTE.ASM`, `MOUSE.ASM` or `HISCORE.ASM`, and never
  inside `detect_prise_option`; the suppression was a port invention.
- **P2 keyboard paddle speed** is re-derived at every ball launch as
  2 × the launch speed (`MAIN.ASM:5345-5348`) — 4/6/8 by difficulty,
  +2 per speed step — instead of the fixed `6` from `MOUSE.ASM:77`,
  which is only the loader initialiser.
- **Credits intro FLC now plays at 12 FPS** (`FILE.ASM:96-99`, five
  vsyncs per frame) instead of 18, matching `FINAL_FPS` in
  `screen_final.c` which already used the same derivation.

- **Music VU meter in the menu** (Blaster.inc:44-52, MAIN.ASM:788-820 and
  649-689) — drag it to set the music level. The empty trough was already
  in the menu art; only the bar and the interaction were missing.

### Removed
- **KITT scanner** on the paddle LEDs (added in 0.1.6) — a non-ASM
  invention, same class as the screen shake removed earlier.
- **Dark side-panel veil.** It existed to hide "bright red side panels in
  the source art" — which turned out to be a conversion artefact, not the
  art. See the background fix below.
- **`M`/`S` audio panel on the pause screen**, the winner/draw banner on
  the duel game-over overlay, the editor key cheat-sheet, and the black
  strip painted under the menu title (it covered the music panel). None
  exist in the original.

### Fixed — the two structural ones
- **Ball speed-ups were 3x too slow.** The port scaled every speed counter
  by 3 to compensate a supposed "ASM 18 fps". The shipped 1999 binary is
  the WIN32/WinEOS build (`MAKEFILE`: `tasm32 … /dWIN32`, `wlink … system
  wineos`), so the DOS branch of `wait_synchro` — VGA `0x3DA` poll *and*
  the PIT pacer — is compiled out (DRAW.ASM:111-152). The real pacer is
  WinEOS `Wait_Vbl`, one frame per vertical blank at 640x480 = 60 Hz, and
  the main loop calls it exactly once per iteration (MAIN.ASM:1097).
  1 ASM frame = 1 port frame. `SPEED_DELAI = 1500` is 25 s at level 1; the
  port made it 75. (18.2 fps is what the never-shipped DOS path would have
  given: 8 ticks of a 145.6 Hz PIT.)
- **World 1 backgrounds were mis-converted — 858 718 pixels.**
  `load_file_fond` calls `Create_Palette` with `ecx = 16*3`
  (FILE.ASM:384-386): only palette entries 0..15 of a background GIF are
  honoured, indices 16..255 come from `spriteN.pal` (FILE.ASM:250-255).
  The eight `01_*` PNGs had been converted through the GIF's own palette,
  where entries 229..243 are one flat red. Regenerated through
  `Sprite1.pal`, they show the blue-grey machinery the original displayed.
  Not one differing pixel was inside the play field.

### Fixed
- **Power-up drop probability was inverted** — `get_random` is inclusive
  (mask + reject, `MAIN.ASM:5103-5127`) but the port used `rand() % N`,
  giving 1/(freq-1) instead of 1/freq: a *guaranteed* drop at freq=2,
  which is 15 of the 23 entries on easy. `COLLISION` (index 23) could
  never be drawn.
- **The campaign is 40 levels, not 80** — `search_level_number`
  (`MAIN.ASM:5025`) stops at the first `0xFF`; each world file holds
  exactly 15 600 `0xFF` bytes. The port ran 40 empty boards before the
  victory screen, and halved the speed ramp along the way.
- **Hard bricks took 7 hits instead of 4** — `draw_brique`
  (`MAIN.ASM:4966-4973`) caps resistance at load time. 294 bricks across
  the three worlds were affected.
- **Demo mode never started**, then froze at level change.
- **Every duel ended in a draw** — `test_game_over` (`MAIN.ASM:4674`)
  ends the match at the *first* player out, with no `dual_flag` test.
- **Per-world palette was not ported** — world 0 rendered with world 1's
  colours (92 palette entries differ). Atlases regenerated from the
  indexed `SPRITE.GIF`.
- **Balls survived 47 px too long** — `detect_destruction`
  (`MAIN.ASM:4526-4541`) kills the ball once its centre reaches the
  paddle's centre line (`pos_y >= 424`); the port waited for it to clear
  the whole 480 px screen.
- **No explosion on losing a life** — `destroy_vaisseau`
  (`MAIN.ASM:4792-4796`) plays `iff_explosion` then `iff_game_over`, and
  `test_game_over` calls it from *both* branches. `SFX_EXPLOSION` was
  loaded and never played anywhere in `src/`.
- **`GHOST`, `ADD_MONSTER`, `MAGNETIC`, `FAST`, `SLOW`, `IRON`,
  `TELEPOD`** — durations and semantics restored (one-shot, permanent
  per ball, real expiry).
- **Rendering** — power-up icons were sampled 2 px into the fade row;
  player 2's ball was blue instead of green (the ASM `+9` is 9 *pixels*,
  not 9 rows); the lives counter ran the wrong way and capped at 9
  instead of 19; score and level were unformatted; the break animation
  used stride 11 instead of 10 and fired at two invented sites.
- **Mouse and keyboard were dead in the web build** since its first
  commit — input is now polled at end of frame.
- **Pause and quit were unreachable by touch.**
- **One carried effect per player, not one per effect.** `SHOOT` + `LARGE`
  + `REVERSE` used to stack. The ASM routes everything through
  `player_current_option` (MAIN.ASM:5689), derives the paddle from it every
  frame, shares one 600-frame timer, and lets any instant powerup strip
  BOTH players. `option_small_ship_p`, `option_large_ship_p` and
  `option_reverse_p` are bare `ret` (MAIN.ASM:6703, 6710, 6717).
- **Attract mode ran only from menu 1 and stalled under a resting mouse.**
  `get_menu` has a single wait loop for all seven menus
  (MAIN.ASM:279-323), and `detect_reset_ecx` re-arms only when the cursor
  position actually CHANGES (MAIN.ASM:569-583).
- **The score table is shown after every game**, not only on a qualifying
  score (`_Display_score` is unconditional, HISCORE.ASM:178-212), and a
  fresh game starts automatically afterwards (`NEW_PLAY` → `@@play_again`
  → `start_new_game`).
- **`final_dual` was shown at victory instead of duel game over** — the
  two HISCORE.ASM entry points were swapped. Both end panels now come from
  the `.cfg` in FR/EN/ES, with the winner label written as four bytes at
  index 16 (HISCORE.ASM:133-138) and the original Spanish alignment bug
  reproduced rather than silently corrected.
- **`.usr` volumes are 0..64 again.** The port collapsed them to booleans
  and overwrote the player's file on exit; the shipped `.usr` holds
  music 34 / sfx 64.
- **Wall bounces no longer move the ball.** `detect_colision_wall` is
  purely predictive and never writes the position (MAIN.ASM:3497-3535);
  snapping shifted the trajectory by up to |v|-1 px per bounce.
- **Monsters draw over balls and paddle**, per their declaration order
  (MAIN.ASM:7096 vs 7066/7087), and the explosion runs 14 advances with
  the first one resetting to frame 0 (DRAW.ASM:396-416).
- **Magnetic catch applies the per-zone angle adjustment.**
  `detect_magnetic_player_1` is a setter that returns; it does not
  short-circuit MAIN.ASM:4219-4243.
- **Sound mapping.** `iff_multi` is a multi-HIT brick surviving its hit,
  aliased onto `wall.iff` — not multi-ball. Invented sounds removed (ball
  spawn, telepod pickup, fast/slow pickup, powerup hitting the ground —
  `iff_lost_option` is never loaded, so it is silent). Teleport,
   four-corner probes now use the full sprite size (MAIN.ASM:1543-1559).
- **Name entry exposes all 45 font glyphs** (FONTE.ASM:418-419) instead of
  37 — punctuation was unreachable, yet the 1999 `.scr` contains
  `pas cool !!!!!`.

---

## [0.1.6] — 2026-04-30 (never tagged)

### Added
- **Powerup pickup banner** in the HUD `panel_info` area.
- **KITT scanner** animation on the paddle's three red lights (pixel-perfect LED crops, slower sweep).
- **Touch buttons** for hiscore name entry (pen / tablet users).
- **Windows `x64` release binaries.** (An earlier version of this entry
  claimed `Win32` and `ARM64` too — `Win32` has never existed in
  `.github/workflows/`, and Windows `ARM64` only arrives with the
  unreleased 5-target matrix above.)

### Changed
- **Audio**: paddle bounce now routes to `WALL.wav` so it no longer echoes the brick-hit sample.
- **Audio**: speedup SFX plays when the natural speed-timer expires.

### Fixed
- **Demo AI** ignores ghost balls and stuck-ghost respawn triggers — paddle no longer pegs the screen edge.
- **Input** clamps cursor X (overshoot no longer freezes the paddle); Win32 cursor is polled directly so focus loss doesn't freeze it either.
- **Powerup**: NIGHT effect was being cleared one frame after pickup.
- **Ghost ball** is destroyed before the paddle bounce flips `vy`.
- **Hiscore** prompt no longer overlaps row 14 at the bottom of the screen.

---

## [0.1.5] — 2026-04-22

### Added
- **Paddle explosion animation on ball loss.** `MAIN.ASM:2353-2409
  destroy_vaisseau` cycles `vaisseau_explo_*` (8 frames × speed 4 =
  32 ticks) on the losing paddle before the ball respawns. Port was
  skipping the animation entirely — losing a life had no visual
  feedback beyond the life counter ticking down. Ball respawn is
  deferred via the new `Paddle.explo_timer` until the animation
  completes; fire input is a no-op during that window.
- **Telepod paddle animation** on `POWERUP_TELEPOD` pickup. Plays
  the 10-frame `vaisseau_telepod_*` sequence (`MAIN.ASM:2420-2460
  Init_Cursor_Telepod`, `Blaster.inc:294-330`) on the collecting
  paddle while the balls make their random jump.
- **Cannon muzzle-flash** overlay while `gun_cooldown > 0`. Draws
  the appropriate `vaisseau_tir*` / `vaisseau_tir_big*` sprite at
  the cannon offset (`Blaster.inc:234-247`), alternating L/R based
  on `paddle.gun_side` for mini shots.
- **Credits intro animation** — the 36-frame `intro.flc` sequence
  (pre-converted to `assets/intro/frame_0001..0036.png`) now plays
  before the 6 credit letters, matching `MAIN.ASM:145-190 @@credit`.
  Frames are lazy-loaded on first Credits entry so cold launch is
  unaffected.
- **In-pause audio toggles** — `M` flips music, `S` flips SFX. State
  lines render above the PAUSED banner; both keys are excluded from
  the generic "any key resumes" check so toggling doesn't leave
  pause. Persists through the existing `blaster.usr` pipeline on
  shutdown.
- **ESC returns to main menu** from the `READY_TO_PLAY` and
  `GAME_OVER` overlays (desktop UX convention — ASM only listened
  for ESC on the main menu, the gameplay loop, and hiscore name
  entry).
- **Brick Blaster logo pinned** at the top of every menu
  (`Blaster.inc:178-182 icon_logo_o` at `(0, 1103, 623, 114)` in
  `MENU.png`). Port was only blitting the `Blaster.png` backdrop.
- **Rounded hover ring** around the selected quadrant in menu 1-7,
  mirroring the disc shape of the menu icon. Replaces the dark
  pulsing rectangle that was a rough RGBA approximation of the ASM
  palette pulse.
- **README** now has a prominent bilingual "Download the game here —
  Téléchargez le jeu ici" link near the top, plus a direct GitHub
  Releases entry in the Links section.

### Fixed
- **HUD life counter was drawn from an empty atlas column.**
  `panel_nbs_ball_1/2_o = (631, 0/9, 9, 9)` points to the 9-px ZERO-
  life anchor slot; `MAIN.ASM:6056-6072 display_nbs_ball` grows ONE
  wide sprite by 12 px per life toward the LEFT, pulling real ball
  graphics from `X=619` down (stride 12). The port drew N
  independent 9x9 sprites from `X=631` — all transparent. Fixed
  the `SR_HUD_LIFE_P1/P2` coordinates to point at the rightmost
  real ball cell.
- **Projectile sprites** confused the paddle cannon recoil animation
  (`vaisseau_tir*`, 12x18 / 13x19) with the actual flying bullet
  (`shoot`, 8x42 at `(444, 339)` and `mini_shoot`, 3x13 at
  `(85, 164)`, per `MAIN.ASM:1947-1990 Detect_Shoot`). Small
  projectiles were rendering the whole cannon-with-muzzle frame,
  making them look identical to big shoot. Now uses the correct
  `shoot_o` / `mini_shoot_o` sprites, centered on the paddle-
  relative spawn point the original decal math computed.
- **Hiscore name entry ENTER key** appeared broken: the user could
  type letters but ENTER did nothing. Root cause — the ENTER held
  during `game_over` skip stayed held through the state transition,
  and `IsKeyPressed()` only fires on rising edges. Added
  `input_wait_click_release()` before the `STATE_HISCORE` transition
  to drain any carried-over press, plus on-screen hints (`"type name
  — enter to confirm — esc to cancel"`).
- **Indestructible brick reflet animation** is now the hit-triggered
  per-brick ripple the 1999 binary did (`MAIN.ASM:4058-4061` stamps,
  `6220-6240` decrements), replacing the ambient whole-screen
  flicker toggle david4599 reported as painful (2026-04-21).
- **Menu hover label** now follows the cursor (`MAIN.ASM:555-567
  refresh_text`), previously centered at `SCREEN_W/2` in the bottom
  bar. Title stays pinned in a tighter 22-px bar beneath it.

### Changed
- Removed the pulsing black overlay on hovered menu quadrants.
  Replaced with the rounded ring + cursor sprite + bottom-bar label
  combination described above.
- Internal refactor: factored out `draw_one_paddle()`,
  `anim_frame_clamp()`, and named `SR_CANNON_*` rectangles so the
  new paddle animations follow the existing `SR_*` convention.

---

## [0.1.4] — 2026-04-21

### Added
- **POWERUP_COLLISION (24/24) is now fully implemented** in 2P modes
  (coop + duel). Pickup snapshots `collision_flag = paddle_2.x -
  paddle.x`; for the next `DELAI_OPTION` (600) frames the two cursors
  push each other instead of overlapping, matching
  `MAIN.ASM:2274-2316 detect_collision_cursor`. Required per-paddle
  `min_x` / `max_x` fields on the `Paddle` struct (mirrors ASM
  `cursor.sprite_min_x` / `sprite_max_x`). Also corrected
  `powerup_duration(POWERUP_COLLISION)` which was wrongly marked
  "instant" — the ASM pickup leaves the effect timed
  (`MAIN.ASM:5687-5691 detect_prise_option`). 1P solo play unaffected.

### Fixed
- **Hallucinated world name "Eclipse"** (world 1 is "Arcade" per
  upstream `Blaster*.cfg menu_text_4b`). Eclipse is the *demomaker
  team* name, not a world name. Removed dead `STR_SPACE` / `STR_ECLIPSE`
  enum entries plus their 12 translations (6 languages × 2) — never
  referenced; the menu already uses `STR_M_SPACE` / `STR_M_ARCADE`
  correctly. Fixed 3 source comments and the stale audit note that
  had the fix backwards.

### Changed
- **README attribution rewritten with sourced facts** (Nov 2024 email
  thread with Marc Radermacher + david4599):
  - Marc proactively transmitted the sources to david4599 in early
    2024 for GitHub publication (was phrased as "convinced Marc to
    open-source the assembly" — no upstream source for that).
  - `BrickBlaster-EOS-Archive` description corrected to also mention
    WinEOS 4.00 Alpha and the standalone EOS 3.05 / 3.06 DOS extenders.
  - Carapace (Softplace) credited as *developer*, Media Pocket as
    *publisher* (previously conflated as "published by Media Pocket
    via Carapace").
  - "Eclipse **demoscene** team" → "Eclipse **demomaker** team" (4
    occurrences) to match upstream wording.
  - WinEOS described as the Windows/DirectX port of EOS (Eclipse
    Operating System), not a generic "DirectX wrapper".
  - Localisation claim fixed: UI is fully translated to 6 languages
    (EN/FR/DE/ES/IT/PT, 104 entries each), not "partial" for DE/IT/PT.
  - david4599 credited by his chosen pseudonym only (his request).
  - Port-author link deduped to a single `[Jonathan Odul
    (konsomejona)](https://github.com/konsomejona)` entry.

---

## [0.1.3] — 2026-04-20

### Fixed
- **NIGHT powerup (dark mode) left `night_active` stuck after being replaced
  by another timed powerup**: our port was setting `current_option = NIGHT`
  + `current_option_count = 600` on collection, and only clearing
  `night_active` in `deactivate_current_option` when the switch case
  matched `POWERUP_NIGHT`. If the player collected e.g. SLOW_BALL while
  NIGHT was active, the NIGHT timer got overwritten and
  `deactivate_current_option` later fired on `POWERUP_SLOW_BALL` — never
  clearing `night_active`, trapping the screen in the dark overlay for
  the rest of the level.
  Re-read MAIN.ASM:5687-5691 (shared collection pipeline) + 6641-6699
  (`option_night_p`) + 6295-6336 (`detect_current_option_end`) +
  6667-6677 (`detect_init_palette`): NIGHT sets `option_night_flag=On`
  and then clears `current_option` back to Off (line 6659), while
  `current_option_count` keeps the 600 frames set by the shared pipeline.
  Palette is restored either when that count hits 0 (`init_palette` from
  `@@current_option_off`, line 6327 → 6684 clears `option_night_flag`)
  or when another timed option is collected mid-NIGHT
  (`detect_init_palette` sees flag On + current_option != Off → clears
  flag, line 6674). Ported: `game.c::apply_powerup` NIGHT case no longer
  sets `current_option = POWERUP_NIGHT`; `deactivate_current_option`
  clears `night_active` unconditionally at the tail (mirrors
  `init_palette`); added `detect_init_palette` equivalent in
  `tick_option_timer`.

---

## [0.1.2] — 2026-04-20

### Fixed
- **Windows taskbar icon still showed TakoHi in 0.1.1**: the runtime
  `SetWindowIcon()` call in `main.c:655` was correctly switched to
  `assets/blaster_icon.png`, but `src/brickblaster.rc` (the Win32 resource
  embedded into the `.exe` at link time, used by GLFW for `IDI_APPLICATION`
  / taskbar / Alt-Tab) still pointed to `assets/takohi/Icon.ico`. Repointed
  to `assets/blaster.ico`. The TakoHi logo remains in the intro splash
  (`screen_intro.c`) — GPL v3 explicitly permits attribution of the porter
  alongside the original work.

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
- Full C99 port of **BrickBlaster** (Media Pocket, 1999, Eclipse demomaker
  team) using raylib 5.0 for rendering / audio / input / windowing.
- Windows x64 (MSVC), Linux x64 (GCC/Clang), macOS arm64, Android (NDK)
  supported.
- 80 original levels across 2 worlds (Space, Arcade), byte-compatible
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
- Intro + victory FLC animations playing at the original 12 FPS cadence
  (FILE.ASM:98 / 164). (The *credits* animation is not part of 0.1.0 — it
  arrives in 0.1.5, and ran at 18 FPS until the Unreleased fix above.)
- Compile flags `TAKOHI_BRANDING` and `GIF_RECORDER` (both `ON` by
  default, both can be turned off for a 100%-vanilla build).
- Three iterations of automated ASM-fidelity audits resulting in
  ~40 P0/P1 fixes documented in `audit-findings.md` and
  `audit-asm-faithful.md`.
- GitHub Actions CI building all three desktop platforms + creating
  release artefacts on tag push.

### Licence
- GPL v3 — inherited from upstream david4599/BrickBlaster.

[Unreleased]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.5...HEAD
[0.1.6]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.5...HEAD
[0.1.5]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.4...v0.1.5
[0.1.4]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/KonsomeJona/brickblaster-c/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/KonsomeJona/brickblaster-c/releases/tag/v0.1.0
