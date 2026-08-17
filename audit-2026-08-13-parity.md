# BrickBlaster — 1999 ASM ↔ C port parity audit (August 13, 2026)

**Code audited**: `61c1dd7` (sources frozen since May 10, 2026).
**Reference**: `david4599/BrickBlaster` — `work400/Blaster/` (MAIN.ASM 7170 lines, FILE.ASM,
DRAW.ASM, FLI32.ASM, HISCORE.ASM, MOUSE.ASM, EDITOR.ASM, FONTE.ASM, Blaster.inc,
Blaster*.cfg) plus the 1999 data files (`Blaster.scr`, `.lv0/.lv1/.lv2`, `.usr`).

**Method**: 6 agents in parallel, one per ASM cluster, each required to cite both sides and
to check the ASM citations found in the port's comments. Every P0 finding below was
**re-verified by hand** afterwards (re-reading the ASM, re-reading the C, measuring against
the real data files).

The April audits (`audit-findings.md`, `audit-asm-faithful.md`, ~40 P0/P1 fixes) were
re-read first; this document does not re-list what is actually fixed.

---

## 1. The most profitable root cause

`get_random` (MAIN.ASM:5103-5127) builds a bit mask covering N, draws, then **rejects
while the result exceeds N**:

```asm
@@cont:     call calc_random
            and eax,ecx        ; masque
            cmp eax,ebx        ; ebx = N
            ja  @@cont         ; recommence si > N
```

So it returns **0..N inclusive** — N+1 values.

The port uses raylib's `GetRandomValue` — inclusive on both ends, hence faithful — in
**14 of the 16** sites. The two exceptions use `rand() %`, which is exclusive, and those
are exactly the two bugs:

| Site | ASM | Port | Consequence |
|---|---|---|---|
| `random_options` MAIN.ASM:5474 | `get_random(23)` → 0..23 | `rand() % 23` → 0..22 | `COLLISION` (index 23) is **never** drawn |
| `random_options` MAIN.ASM:5511 | `get_random(freq-1)` → 0..freq-1, spawn on 0 → **1/freq** | `rand() % (freq-1)` → **1/(freq-1)** | freq=2 ⇒ `rand()%1==0` ⇒ **guaranteed drop** |

On easy difficulty, 15 of the 23 entries have `freq=2`: every eligible brick drops a
power-up, where the 1999 game dropped one out of two.

`powerup.h:47` even states the rule correctly ("probability = 1/(freq-1+1) =
1/freq"); `powerup.c:274` writes the opposite three files away, and that is the version
that got coded.

**Fix**: replace both `rand() % N` with `GetRandomValue(0, N)`.
`powerup.c:236`, `powerup.c:275`, `game.c:160`, `game.c:168`.

---

## 2. P0 — divergences that change the game

### P0-1 · The game has 40 levels, the port plays 80
`search_level_number` (MAIN.ASM:5025) counts up to the first `0xFF` byte.
**Measured**: each world file is 31,200 bytes, of which **exactly 15,600 bytes are
`0xFF`** — 40 real levels, 40 empty slots. The port hardcodes `LEVELS_PER_FILE 80`
(`level.h:24`) and justifies that 80 with "validated by FILE.ASM:1205" — that line gives
the *file capacity*, not the number of playable levels.

*Effect*: after the real level 40, 40 empty boards follow one another ("ready ?", one
shot, end-of-level jingle) before the victory screen.
*Fix*: compute `level_number` at load time by stopping at the first `0xFF`.
Also fixes P0-3 below. `level.c`, `main.c:245`, `game.c:321`.

### P0-2 · Hard bricks take 7 hits instead of 4
`draw_brique` (MAIN.ASM:4968) caps at load time: any brick with resistance > 1 enters
play at exactly 4 (`and ebx,nombre_de_coups` / `or ebx,100b`). The port reads the literal
HP (`brick.c:48`).
**Recounted across all 3 worlds**, replaying the full ASM logic (including the
transparent→normal remap that precedes the test): **294 bricks** (`0x27`, `0x67`, `0xA7`,
`0xE7`) are worth 7 instead of 4. The 331 `0xE4` bricks and the ~1,800 transparent ones
come out right.
*Fix*: apply the cap in `brick_decode`.

### P0-3 · The speed ramp kicks in twice too late
Same cause as P0-1: `game.c:321` divides by 80 instead of 40. Made worse by
`change_speed_level` coded `3/4/4` (`game.h:108`) while `Blaster.cfg:45` ships `(2,3,3)`
— the parser does read the key, `main.c:428` just never injects it.
*Effect*: the whole late campaign is slacker than in 1999.

### P0-4 · Demo mode never starts
The ASM goes straight to `PLAYING` with a fixed velocity (`sens_x=+3, sens_y=-4`,
MAIN.ASM:2761). The port enters `STATE_READY_TO_PLAY` (`screen_menu.c:149`) and waits for
a shot that will never come — `game.c:1854` requires `p1_fire || p2_fire`, and the early
return at `game.c:1899` even prevents the demo timer from counting down. A comment
(`demo.h:46`) points to "the `@@demo` branch of main.c", which never existed.
*Effect*: attract mode shows a board frozen on "ready ?".

### P0-5 · Every duel ends in a "draw"
`test_game_over` (MAIN.ASM:4674): `dec player_nbs_ball / cmp -1 / jne @@cont` → the
**first** player to run out ends the match, with no `dual_flag` test at all. The port does
`game_over = p1_dead && p2_dead` (`game.c:1276`), the dead player keeps receiving balls,
and since both are dead at the end, `winner` stays `-1`.

### P0-6 · Three broken power-ups
- **`GHOST`**: ghost balls explode on the first brick (`game.c:2205`); in the ASM the
  ghost test exists **only** in the paddle collision (MAIN.ASM:4207) — they break bricks
  and only die on contact with the ship. The port's `continue` additionally skips the
  update of the brick that was hit (inconsistent state).
- **`ADD_MONSTER`**: goes through the natural spawn counter (`monster.c:55`) instead of
  invoking directly like MAIN.ASM:6764 — near no-op, and it delays the next spawn.
- **`MAGNETIC`**: never expires. The `case` that disables it (`game.c:669`) is
  unreachable because `apply_powerup` explicitly excludes `POWERUP_MAGNETIC` from
  `current_option` (`game.c:1134`).

### P0-7 · `FAST` / `SLOW` are 10-second ramps
The ASM resets `current_option` to `Off` at the end of `Refresh_Ball` (MAIN.ASM:2885):
the effect survives only **one** pass, i.e. a single speed step. The port classifies them
"timed, 600 frames" (`powerup.c:318`) and applies a delta every 3 (slow) or 32 (fast)
frames. "Slow" pins the ball at minimum speed for 10 s.

### P0-8 · The per-world palette is not ported — the default world has the wrong colors
`MAIN.ASM:97,483,491` patches the file name (`sprite0.pal` / `sprite1.pal`) per world,
and `Read_Palette` (FILE.ASM:776) replaces the sprite sheet's 768 palette bytes on every
load. The port loads a single `SPRITE.png` (`assets.c:57`) and never repalettizes
anything.

**Measured**: the two palettes differ on **92 entries** (indices 32-143). After VGA
6→8-bit conversion (×4), **191 of the 191 colors** in the shipped `SPRITE.png` belong to
`Sprite1.pal`, versus 104/191 for `Sprite0.pal` — the shipped sheet is world 1's.

*Effect*: in world 0 ("blaster", the default world), bricks, balls and paddles had a
distinctly different tint in 1999. The port shows the "arcade" colors in both worlds.
*Fix*: ship two sprite sheets, or apply the palette when loading the world.

---

## 3. P1 — correctness gaps

- **Power-ups collectible during "ready ?"** — `detect_prise_option` bails on its very
  first line outside `PLAYING` (MAIN.ASM:5605); the port's loop (`game.c:2528`) is not
  gated. **This is the most direct candidate for the player-reported bug** ("a ball
  appeared out of nowhere"): after a lost life, a leftover multiball falls onto the
  motionless ship and launches balls while the main one is still stuck. The `SPAWN_LOG`
  statements left in the last commit should show it.
- **Spawn window not re-armed on failure** (`game.c:1544`) — the ASM consumes the window
  even when the draw fails (MAIN.ASM:5466). Amplifies the power-up rain.
- **One option slot per player in 1999** — the ASM reads every carried effect through
  `player_current_option`: a player can only have one effect at a time, any instant
  power-up cancels the effects of **both** players, and the 600-frame timer is shared.
  The port has independent per-paddle timers: `SHOOT` + `LARGE` + `REVERSE` stack.
  The April fixes repaired P2 routing but went beyond the ASM.
- **Missing death line** — `detect_destruction` (MAIN.ASM:4526) dooms the ball at
  `y ≥ 424`, 8 px below the top of the paddle; the port allows saves down to
  `y ≥ 471`. The port is markedly more permissive.
- **P2's ball is not launched mirrored** (MAIN.ASM:5309 `neg eax`) — both balls
  start off rightward (`game.c:1857`).
- **P2 keyboard paddle speed fixed at 6** — the ASM recomputes it at every launch as
  2×(ball speed) (MAIN.ASM:5345), i.e. 4/6/8 by difficulty, +2 per step.
- **Magnetic catch**: the port exits before the per-zone angle adjustment
  (`collision.c:612`) that the ASM still applies (MAIN.ASM:4214).
- **Back to menu after game over** — the ASM **automatically** restarts a fresh game
  (`NEW_PLAY` → `@@play_again`, MAIN.ASM:1194) after the score table, which is always
  displayed. The port falls back to the menu, and only shows the table if the score
  qualifies.
- **Attract mode nearly unreachable** — the port limits the timeout to menu 1 and zeroes
  the counter as soon as the cursor hovers a quadrant (`screen_menu.c:260`); the ASM
  counts down in every menu and only resets when the position **changes**.
- **High-score names in uppercase** (`screen_hiscore.c:251`) — `HISCORE.ASM:316` forces
  `or al,20h`, and the 1999 font has no uppercase. A `.scr` written by the port would
  display empty names in the DOS binary.
- **BOOM and DEATH missing on life loss** — `destroy_vaisseau` (MAIN.ASM:4793) plays
  both on **every** death. `SFX_EXPLOSION` is loaded (`audio.c:58`) and **never played
  anywhere** (checked: zero calls in all of `src/`).
- **Rules suspended in demo** — score, power-ups and lives are neutralized when
  `demo_active` (`game.c:1520, 2452, 2543`); the ASM has no `demo_flag` guard in
  `inc_score` or `detect_prise_option`. The 1999 attract mode is a real game.

### Rendering

- **P2's ball green, not blue** — `MAIN.ASM:2803`: `ball_orange_o` then `+9` for P2 in
  duel, i.e. +9 **pixels** in the atlas = the green ball (golden under iron, green ghost).
  The comment at `draw.c:570` justifies the inversion by claiming that "ball_orange_o is
  at offset 9" — it is `640*9 = 5760`. The duel's entire color logic rests on that
  reading. On top of that, the port picks the color by **ball index** (`i >= 1`) rather
  than by owner.
- **Falling power-up icons corrupted** — `POWERUP_SPRITE_Y_OFF` (`draw.c:139`)
  sends BALL_3/6/9/20 to Y=804, i.e. 2 px into the "fade" row (802), plus vertical
  flips and a mirror. The ASM always blits row 752, intact; the fade row only serves
  the HUD icon. Fix: `Y_OFF` and the flip tables to zero.
- **Active power-up icon missing from the HUD** — `MAIN.ASM:5691` places the icon at the
  bottom left for the whole duration of the effect; the port shows only text.
- **"Break" animation: stride 11 instead of 10** (`draw.c:952`) — the ASM advances by
  `size_x + next_shape` = 9+1; the frames are therefore read askew, with a cumulative
  offset of up to 4 px. Worse, the animation is triggered on **every destroyed brick and
  every lost ball**, with a fabricated ASM citation: the only two original sites are a
  ghost ball bursting on the paddle.
- **Life counter in the wrong direction** — the ASM stacks the balls to the **left** in
  steps of 12 from x=518; the port goes right in steps of 11 and overflows onto the side
  panel from the 2nd life on. The P2 row is moreover shown in co-op, while the ASM
  reserves it for duel.
- **Score and level unformatted** — the ASM displays `000150` and `01` at the panel's
  exact width; the port does a centered `%d`.
- **Credits FLC at 18 fps instead of 12** (`screen_credits.h:16`) — `FILE.ASM:96` waits
  5 vsyncs per frame. Of note: `audit-asm-faithful.md` listed this item under its
  *Confirmed matches*.
- **Border pillars missing and palette blackout not ported** — `create_border`
  (MAIN.ASM:5825) stamps 5 tiles of 42×96 at x=70 and x=528, and `load_file_fond` forces
  entries 0 and 15 of the background palette to black. The port replaces all of it with a
  dark semi-transparent overlay "for a polished look on mobile" — which unknowingly
  compensates for the unported palette trick.
- **"KITT" LED scanner on the paddle** (`draw.c:695`) — non-ASM addition owned up to in
  a comment, same class as the screen shake purged in April under the HARD rule.
- **Monster explosion 2× too short** — the ASM advances one frame every 2 vsyncs
  (~30 frames); the port decrements every frame (~15). The −19 offset contradicts its own
  citation ("sub pos,16").
- **Ghost P2 score in co-op** (`draw.c:846`) — an orphan "0" at x≈4, outside the panel;
  `FONTE.ASM:10` only prints it in duel.

---

## 4. P2/P3 — presentation and hygiene

Power-up banner shown for 600 frames instead of 100 (`DELAI_INFO` defined in
`constants.h:148`, never consumed) · `.usr` reduced from 0..64 volumes to booleans, panel
VU meter missing · `final_text`/`final_dual` victory screens hardcoded in English while
all three `.cfg` files carry the FR/ES versions · `final_dual` shown on victory instead
of on duel game over · invented sound on a power-up lost on the floor (`iff_lost_option`
is `0` in the ASM = silence) · `iff_multi` mismapped · no fade on transitions ·
wall bounce snapped in position (the ASM never touches position) · monster explosion
offset by 19 px instead of 16 · `last_random` (never the same draw twice) not ported ·
teleport 1 px too short.

**Power-up banner: the audit was right, my §6c fix was wrong.**
`DELAI_INFO` IS used: `dec current_option_count / cmp current_option_count,DELAI_INFO /
je @@display_info_off` (MAIN.ASM:6304-6306), with `DELAI_INFO = DELAI_OPTION-100 = 500`
and the counter armed at 600 — so the banner lasts **100 frames**, for all power-ups. My
"not referenced in the ASM" in §6c came from an empty `grep`, and `grep` is broken on
these files (see the method lesson closing §6e). Fixed in the port.
`DELAI_INFO_SOUND`, on the other hand, really is dead: its only occurrence
(MAIN.ASM:6307) is commented out.

Dead constants on the port side: `CREDITS_SLIDE_TIMEOUT`, `DELAY_INTRO_1`, `SB_FREQ`.
Dead i18n strings: `STR_READY`, `STR_GAME_PAUSED`, `STR_GAME_OVER`, `STR_DEMO_LABEL`.

---

## 5. The pattern to fix in the method, not just in the code

Three findings are April fixes that **enshrined an assumption that was never verified**:

- The P0-5 fix (duel) — numbered **P0-2** in `audit-findings.md`, whose own P0-5 is a
  different, scoring-related item — carried the note "ASM disables losing paddle but keeps game until
  BOTH exhausted — **verify**". The verification was never done; the opposite got coded.
- `final_dual` was fixed **in the wrong place** (victory instead of game over).
- `IRON BALL` was marked "to be decided" and then timed at 600 frames while the ASM makes
  it permanent per ball.

Several port comments cite the ASM with confidence at places where the cited line says
something else: `main.c:238` invokes a `new_play` label that does not exist;
`powerup.c:236` claims "range is 0..22"; `monster.h:13` is off by two lines;
`audio.c:44` declares itself a "port deviation" on behavior that is actually faithful.

**Proposed rule**: no fidelity fix gets merged unless its comment cites an ASM line that
was opened and re-read. An unverified citation is worth less than no citation — it makes
an assumption look like a measurement.

---

## 6. Suggested fix order

1. **`GetRandomValue` at the 2 sites** — 4 lines, fixes the guaranteed drop + `COLLISION`.
2. **`level_number` computed from the `0xFF` sentinel** — fixes the campaign ending and
   the speed ramp in one go.
3. **4-hit cap in `brick_decode`** — 3 lines.
4. **Gate power-up collection on `STATE_PLAYING`** — probably fixes the player-reported
   bug; would allow removing the `SPAWN_LOG` from the last commit.
5. **Demo: enter `PLAYING` with the fixed velocity of MAIN.ASM:2761.**
6. **Duel: game over on the first player to run out.**
7. **Power-up icons**: `POWERUP_SPRITE_Y_OFF` and the flip tables to zero — one line
   each, fixes visibly damaged icons.
8. **"Break" animation**: stride 10, and remove it from the two invented triggers
   (brick destruction, ball loss).
9. `GHOST`, `ADD_MONSTER`, `MAGNETIC`, `FAST`/`SLOW` — one by one, each with its citation.
10. **Per-world palette** — heavier (two sprite sheets to produce, or a repalettization
    at load time), to be planned separately.
11. The rest by decreasing severity.

Items 1 to 3, 7 and 8 are very small diffs for a large gameplay effect. Item 4 is the
only one answering a real player report.

---

## 6b. What has been fixed (branch `fix/asm-parity-2026-08`)

Two waves of fixes, separated by an **adversarial** verification pass whose brief was to
look for what was still wrong, not to confirm.

**Wave 1** — the fixes listed above. Verification showed they had introduced
**one regression** and left **five fixes half-done**:
- `draw.c` measured a brick's wear as `raw − hp`; since the cap changed `hp` without
  changing `raw`, the 294 affected bricks rendered as three-quarters destroyed
  **from the moment the level appeared**. Fixed with an `hp_initial` field.
- The demo started but **froze at the first level transition** (`NEW_PLAY` forced
  `READY_TO_PLAY`; the ASM jumps past it, MAIN.ASM:961-967).
- `IRON`, `GHOST` and `TELEPOD` were declared one-shot but the tail of the pipeline still
  set `current_option` + 600 frames and canceled them anyway.
- The speed ramp had become **too fast**: the divisor was fixed, but
  `change_speed_level` was still 3/4/4 instead of the cfg's `(2,3,3)` — 60 cells out of 120.
- The spawn window was still not consumed on failure (×2.48 on hard).

**Wave 2** — the points above, plus: P2 ball mirrored, burst filtered by owner in duel,
four burst sprite families, animation cadence, life counter up to 19, double KO in duel
(the ASM knows no draw — HISCORE.ASM:133-140 tests only P2's counter).

**Random generator — decision: faithful port.** `calc_random`/`get_random` are ported
bit for bit in `asm_random.c`, flaws included: `alea1` is never written, the CMOS reads
are commented out, the period is 4811 with the first cycle at 2603.
Equivalence proven over 100,000 draws against an independent x86 emulator. Cost measured
and accepted: index 9 comes out +23.4% above uniform, the rare ones at −7%.
Since `wait_synchro` (DRAW.ASM:110) advances the generator **every frame**, the tick is
ported as well — but trace-for-trace parity remains out of reach, the 1999 VGA cadence
not being the port's. What is secured is the statistical grain.

`create_ball` and `Random_Speed` are ported too: multiballs now leave with randomly drawn
positions and directions, no longer in a deterministic fan. **This is the most noticeable
change controller in hand.**

Deliberately not ported: the random background of the score screen (a port invention,
with no ASM site — wiring it up would pollute the shared global state).

## 6c. P1 backlog handled on August 16, 2026 — and verified at runtime

This pass closed the gap declared in §7 ("no finding was confirmed by running the
game"). The port was compiled and **launched** under WSLg, and the visual fixes were
confirmed on captures of the actual window.

**Fixed, with every ASM line reopened:**

- **`SFX_EXPLOSION` + `SFX_GAME_OVER` on every life loss.** `destroy_vaisseau`
  (MAIN.ASM:4792) opens with `play_sound iff_explosion` / `play_sound iff_game_over`, and
  `test_game_over` calls it from **both** branches — MAIN.ASM:4685 (game over) and
  MAIN.ASM:4703 (`@@cont`). `SFX_EXPLOSION` was played nowhere.
- **Credits FLC 18 → 12 fps.** FILE.ASM:96-99 `mov ecx,5` / `loop @@wait`. Identical to
  FILE.ASM:164-170, from which the port already derived `FINAL_FPS 12.0f`: the 18
  contradicted the port's own sister constant, and its comment ("standard FLC playback
  rate") was an invention — the rate is whatever the wait loop counts.
- **P2 keyboard speed.** MAIN.ASM:5345-5348 recomputes `speed_counter` as
  `2 × |ball_2.sens_x|` at every launch (4/6/8 by difficulty, +2 per step).
  `MOUSE.ASM:77 speed_counter dd 6` is only the loader's initializer.
- **Death line.** `detect_destruction` (MAIN.ASM:4526-4541) kills the ball as soon as its
  **center** reaches `limite_y + cursor_size_y/2` = 416 + 12 = 428, i.e. `pos_y ≥ 424`,
  and only while descending (`js @@end`). The port waited for the ball to leave the
  screen, `pos_y ≥ 471` — 47 px of free catch-up.
- **`demo_active` guards removed (5 sites).** Decisive check: `demo_flag` appears in
  **no** other module — zero occurrences in FONTE.ASM (`inc_score`), MOUSE.ASM and
  HISCORE.ASM — and never in `detect_prise_option` or `test_game_over`.
  The 1999 attract mode scores, collects and applies power-ups.
- **Border pillars.** `create_border` (MAIN.ASM:5825-5852), 5 tiles of 42×96 per side at
  x=70 and x=528. The port's dark overlay is kept — it compensates for the palette trick
  of `load_file_fond`, which is not portable — but the pillars are now drawn on top.
- **Active power-up icon in the HUD.** MAIN.ASM:5688-5694, at `panel_option` (122, 446),
  sampled from `option_fade_o` (row 802). It is the sole consumer of that row, which
  confirms after the fact that the falling icons must never touch it.

**Outside parity, found while checking the README's controls table:** `A` and `D` were
bound **both** to P1 and P2 in 2-player keyboard mode (`input_frame.c`) — a single key
moved both paddles. P1's aliases are disabled when P2 is on the keyboard. The 1999 P1 had
no keyboard binding at all (`Refresh_Mouse`).

**Two errors of this audit itself, corrected:**

- ~~§4 was wrong about the power-up banner~~ — **that correction is the one that was
  wrong**, and it deserves to be kept as a warning. I had concluded "`DELAI_INFO` is
  referenced nowhere in the ASM" from an empty `grep`. But `grep` silently returns zero
  results on these files in this environment: `DELAI_INFO` is indeed read at
  MAIN.ASM:6305 and sets the banner duration to 100 frames. §4 was right.
  Since then, every search in the ASM goes through `awk` and a direct read.
- §3 gave the death line as "`y ≥ 424`, 8 px below the top of the paddle". The
  threshold is indeed 424, but the gap is 12 px (`cursor_size_y/2` = 25/2), and the
  comparison is on the ball's center, not its edge.

**New open question — the side overlay.** While placing the pillars we checked
`load_file_fond` (FILE.ASM:374-386): it zeroes entries **0 and 15 of the palette of the
whole background GIF**, then calls `create_border` (FILE.ASM:388), then copies into
`background_buffer`. So it is **not** a darkening of the side columns, and the port's
semi-transparent overlay is not its equivalent — its real justification, written in the
port, is to hide the source art's bright panels. Since the pillars only cover 70..112 and
528..570, the overlay stays visible on either side: authentic art sitting on an invented
darkening. The overlay should probably go, with the palette blackout redone at load time
in `assets.c` (a precedent exists in the same place for the logo). A visual change to be
validated on its own, not done here.

**Still open** (unchanged): single option slot per player, `Shade_On/Off` fades,
automatic restart after game over, score table always displayed, lowercase high-score
names, `final_text`/`final_dual` hardcoded in English, sound-settings screen not wired.
*(All of these except the `Shade_On/Off` fades were closed in §6d the same day — the
list above is what was true at the end of §6c, kept as written.)*

## 6d. Full-parity pass — August 16, 2026

Goal: close **all** remaining divergences. Four clusters briefed in parallel (rules,
audio, rendering, texts/saves), each verdict delivered citing both sides. Everything
below is applied, compiled and verified at runtime.

### The two structural discoveries

**The frame rate was wrong by a factor of 3.** The `MAKEFILE` assembles `tasm32 … /dWIN32`
and links `system wineos`: the 1999 binary is the **WIN32/WinEOS** variant, so the whole
DOS branch of `wait_synchro` — the VGA `0x3DA` poll *and* the PIT pacer
`timer_counter >= 8` — is compiled out of the binary (DRAW.ASM:111-152,
`ifndef WIN32 … else … endif`). The real pacer is WinEOS's `Wait_Vbl`: **one frame per
vertical blank at 640×480, i.e. 60 Hz**, and `wait_synchro` is called only once per loop
iteration (MAIN.ASM:1097).
The port compensated for an "18 fps ASM" with a ×3 on 7 sites — 18.2 fps is what the DOS
path gave (8 ticks of a 145.6 Hz PIT), code that never shipped. Measurable consequence:
`SPEED_DELAI = 1500` is 25 s at level 1, the port made it 75. The mapping is **1:1**,
as confirmed by `DELAI_OPTION = 600` and `DELAI_DATTENTE = 600` (10 s each), which the
port already used as-is. Proof recorded at the top of `game.h`. The "70 Hz VGA" cited in
§7 is also wrong: 640×480 refreshes at 60 Hz, 70 Hz is mode 13h.

**World 1's backgrounds were bad conversions.** `load_file_fond` calls `Create_Palette`
with **`ecx = 16*3`** (FILE.ASM:384-386): only entries 0..15 of the background GIF's
palette matter, indices ≥ 16 are rendered with `spriteN.pal`, loaded once per world
(FILE.ASM:250-255). The 8 `01_*` PNGs had been converted through the GIF's palette, where
entries 229..243 are a single flat red — hence the "bright red panels of the source art"
that the port's side overlay had been invented to hide. **858,718 pixels corrected** by
regenerating through `Sprite1.pal`; 0 pixels of difference in the playfield, all the
differences were in the panels. The overlay is removed and the original blue-gray
machinery shows through. World 0 was already correct (5 px on `00_07`). Also verified:
`Blaster.png` (menu) is legitimately a naive conversion, `Load_Picture_Menu` using
`Create_Palette` with `ecx = 256*3` (FILE.ASM:216-220).

### Game rules

- **Single option slot per player.** `player_option[2]` + `sync_paddle_from_option`,
  derived every frame like `detect_large/small_cursor_*` and `detect_shoot_*`
  (MAIN.ASM:1071-1076). Proof that no per-paddle counter exists:
  `option_small_ship_p`, `option_large_ship_p` and `option_reverse_p` are bare `ret`s
  (MAIN.ASM:6703, 6710, 6717); the `count_tir_*` are cannon animation counters
  (MAIN.ASM:1897-1903). Any instant power-up wipes **both** players, the 600-frame timer
  is shared, `MAGNETIC` restores `old_option` in all three slots
  (MAIN.ASM:6745-6750), and the big shot is consumed (MAIN.ASM:1947-1948, 2156-2157).
- **Attract mode** in **all** menus (`get_menu` has a single loop,
  MAIN.ASM:279-323), re-armed only when the cursor position **changes**
  (`detect_reset_ecx`, MAIN.ASM:569-583) — a motionless hover used to block the demo
  indefinitely. `control_2 = COMPUTER` reset on timeout (MAIN.ASM:314).
- **Magnetic catch**: `detect_magnetic_player_1` is a plain setter that `ret`s
  (MAIN.ASM:4398-4439), it does not interrupt the sequence — so the caught ball also
  gets `neg sens_y` and the per-zone angle adjustment (MAIN.ASM:4219-4243).
- **End of game**: the score table is **always** displayed
  (`_Display_score` is unconditional, HISCORE.ASM:178-212; only the name entry is
  conditional, HISCORE.ASM:235-237 / 281-282), and a fresh game is restarted
  automatically (`NEW_PLAY` → `@@play_again` → `start_new_game`, MAIN.ASM:1092-1093,
  1194-1211, 995-1028) instead of returning to the menu.
- **Co-op score**: verified **conforming**. `inc_score` forces `ebp = player_1` outside
  duel (FONTE.ASM:88-91), and the port only splits the counters when `game_mode == 2`.

### Audio

Table rebuilt from the **label aliases** of FILE.ASM:728-770, cross-checked against
17 `loadsample` for exactly 17 `.IFF` files on disk:

- `iff_death` ≡ `iff_game_over` → `death.iff`;
- `iff_incassable` ≡ `iff_cursor` ≡ `iff_multi` → `wall.iff`;
- `iff_lost_option` has **no** `name_iff_*` entry → never loaded → **silence**.

Fixed: `iff_multi` is a **surviving multi-hit brick** (`cmp B [esi+ebx],absente /
jne @@redraw_brique`, MAIN.ASM:4004-4005), not a multiball — `SFX_MULTI_BALL` removed,
the brick sound now has its three cases. Invented sounds removed: ball spawns,
`TELEPOD` on the power-up, `SPEEDUP` on `FAST`/`SLOW` (all four handlers are `ret`s,
MAIN.ASM:6613-6636), and a power-up hitting the floor. `SFX_POWERUP_LOST` renamed
`SFX_DEL_MONSTER`: it already played the right file under a misleading name. The
`audio.c` comment declaring itself a "port deviation" on the paddle bounce was wrong —
the behavior was faithful.

### Rendering

- **Wall bounce**: `detect_colision_wall` (MAIN.ASM:3497-3535) is purely predictive and
  **never** writes the position; the port snapped the ball to the wall, shifting its
  trajectory by up to |v|−1 px on every bounce.
- **Z order**: monsters are declared after balls and ships
  (MAIN.ASM:7096 vs 7066/7087) and `Draw_sprites` walks the table in memory order
  (DRAW.ASM:426-434) — the 70×70 explosion went under the paddle.
- **Monster explosion**: stop at `to_delete == 1` (14 advances, DRAW.ASM:396-400) and
  the first advance is a **reset**, not an increment (`cmp current_shape,1 / jbe @@reset`,
  DRAW.ASM:403-416, `current_shape` initialized to 1 by MAIN.ASM:3080).
- **Teleport**: the four probes use the sprite's **full** size
  (MAIN.ASM:1543-1559), not size−1.
- **Power-up icons**: `get_powerup_rect` exposes the atlas's three rows as named
  offsets (`OPTION_ROW_FALL/P2/FADE`) instead of an after-the-fact mutation.

### Texts, scores, saves

- **`final_text` / `final_dual`** in FR/EN/ES from the `.cfg` files (`read_text_final`,
  FILE.ASM:1089-1110), `winner` patched via a **4-byte `memcpy` at index 16**
  (HISCORE.ASM:133-138) — the Spanish `.cfg`'s alignment bug reproduced as-is.
  Exact geometry: x = 120, y = 52, line spacing 30 (FONTE.ASM:171-184, 370, 416), drawn
  over the last FLC frame (`transparence = On`) and not on black.
- **`final_dual` moved from victory to duel game over**: `Display_score_from_final`
  exits on `dual_flag` (HISCORE.ASM:28-29) while `Display_score` shows `final_dual`
  **only** in duel (HISCORE.ASM:109-141). The port had the two inverted. The overlay's
  "player 1 wins / draw" banner is removed: it does not exist, and a draw is
  impossible (HISCORE.ASM:134 tests only P2).
- **`.usr` volumes**: two 0..64 bytes (FILE.ASM:815-816), carried through end to end.
  The port reduced them to booleans and **overwrote the player's file on exit**. The
  menu's **music VU meter** is implemented (Blaster.inc:44-52, drawing
  MAIN.ASM:788-820, drag `detect_button_music` MAIN.ASM:649-689); the pause `M`/`S`
  panel, which does not exist in the original, is removed. The full-width black band the
  port painted under the menu title hid the VU meter: removed, the title is printed at
  `panel_menu` = (155, 446) over the art as in the original.
- **Score entry**: the wheel exposes the font's **45 glyphs**
  (FONTE.ASM:418-419) instead of 37 — punctuation was unreachable while the 1999
  `.scr` contains `pas cool !!!!!`.
- **Editor**: the two invented help lines are replaced by the `.cfg`'s single
  `option_text_editor` banner (EDITOR.ASM:54-55).
- **Verified conforming**: the XOR codec and `.scr` format (the 1999 `.scr` decodes
  correctly with the port's algorithm — a file written by the port is readable by the
  DOS binary), lowercase names, and the 24 power-up labels in FR/EN/ES, compared byte
  for byte against the `.cfg` files (87 strings, 0 differences). One single correction:
  `cooperado.` in ES.

### Errors of this audit corrected

- ~~`DELAI_INFO` / `DELAI_INFO_SOUND` are dead **in the ASM too**~~ — this correction was
  itself wrong, and the later pass reversed it: `DELAI_INFO` is read at MAIN.ASM:6305 and
  sets the banner to 100 frames. See §6e. Only `DELAI_INFO_SOUND` is genuinely dead, its
  single occurrence (MAIN.ASM:6307) being commented out.
- The death line is indeed at 424 but the gap is 12 px (`cursor_size_y/2`) and the
  comparison is on the ball's **center**.
- The "70 Hz VGA" of §7: the resolution is 640×480, hence 60 Hz.
- The claim of an invented sound on the paddle bounce: the port was faithful.

### What will stay out of reach

The `.iff`/`.mod` → WAV audio conversion is not reversible bit for bit, and the port's
GIF/LZW decoder is not the 1999 one. These two items cannot be "fixed", only documented.


## 6e. Double cross pass ASM ↔ C — August 16, 2026

Two agents launched in parallel, one per direction, forbidden to assert anything without
having opened both sides. Every finding retained below was then **reopened by hand**
before being fixed.

### ASM → C direction (omissions) — 16 findings out of 17 applied

The heaviest: **losing a life reset almost nothing**. `test_game_over` exits with `stc`
on the "keep playing" branch and the loop does `jc start_game` (MAIN.ASM:1089);
`start_game` (1036-1045) calls `init_sprites` — `sprite_status,Off` on **all** sprites
outside the panel — plus `reset_magnetic` and `reset_ghost`, then falls into
`rebuild_all` → `init_monster`. The port only cleared the power-ups. Same path on level
change (`_next_level` ends with `jmp start_game`, MAIN.ASM:968).

Then: **a single click launches both balls** (`detect_start_game` has only one
`call read_click`, MAIN.ASM:5286, and `read_click` aggregates mouse/CTRL/joystick); **the
big shot goes through everything**, unbreakables and monsters included
(`sprite_rebond,Off` MAIN.ASM:1946 → never a `new_direction` 3863-3864 →
`change_direction` exits before `@@shoot_off`); **in duel the option is reserved for
whoever broke the brick** (5626-5630), with the easter egg that **gives it to the
opponent** if the collector is holding their button (5655-5670); **5 monster slots**,
not 4 (`nbs_monster = 4` appears only in a commented-out line,
MAIN.ASM:2932); monsters bouncing off transparent bricks (3993-3994 + `stc`); iron
ball passing through teleporter bricks; GHOST redone (`set_ghost` marks **all** the
collector's balls, `unghost_one_ball` unmarks one); shot positions (18 px above the
paddle for both types, big shot centered on the **current** width); re-roll on
frequency 0 (`@@again` is a loop, 5508-5509); `speed_delai` seed; automatic fire
in demo (`@@auto_shoot`, 1907-1920); `.cfg` scalars injected; option timer frozen
outside `PLAYING` (6298-6300); **power-up banner at 100 frames** (see below).

Not applied, accepted: the space bar restarting a fresh game (MAIN.ASM:1173-1181)
— Space is fire in the port.

### C → ASM direction (hallucinations) — 2,025 citations extracted, ~620 verified

Observed rate: **~88% exact, 7-8% off by ±1-4 lines, 4-5% outright wrong.**

**One dead gameplay bug, introduced by the option-slot refactor**: `POWERUP_NIGHT`
canceled itself. Its `case` set `night_active = 1`, then the `default:` branch of the
second switch reset it to 0 in the same call. The pickup played its sound and no longer
did anything. The `night_active = 0` actually only concerns iron / telepod / fast / slow,
whose `@@reset_current_option` (MAIN.ASM:2895-2898) leaves `current_option` visible for
one frame to `detect_init_palette` (6667-6677).

Other substantive fixes: the **extra life cancels both players' option**
(`option_new_life_p`, MAIN.ASM:6441-6443); the **iron ball triggers sound and reflection**
on the unbreakable (`@@collision` plays `iff_incassable` and arms the reflection whatever
the bounce, 4048-4062); the per-ball order is **paddle → wall**, not the reverse
(2864-2871).

**Wrong citations fixed**: ~20 `Blaster.inc` numbers systematically shifted in
`draw.c` (values right, lines wrong — `brique_classic_o` 342→345, `brique_beton_o`
350→342, `vaisseau_large_1_o` 281→284…); the **`load_decor` label that exists nowhere**
(the real path is `next_fond` → `load_file_fond`); `MAIN.ASM:347` recycled in 7 places
for in-game pickup when it is the **menu click** (the real site is 5708);
`MAIN.ASM:133-138` instead of `HISCORE.ASM` for the `winner` patch; a dozen
off-by-1-to-4s.

**Two entirely invented justifications, removed**: the port claimed the ASM never
reaches `detect_game_over` in demo — with **two different, contradictory mechanisms**
depending on the file ("demo_timer exit" / "read_click exit"). Neither exists:
MAIN.ASM:1088 is unconditional and `test_game_over` has no `demo_flag` guard. The 1999
attract mode really does lose its lives. The port's respawn is kept but is now
**labeled as a deviation**, not as fidelity. Likewise, the explanation of the trajectory
table's `sar eax,1` ("half-step before the ball's edge") was wrong: the result is
`pixel<<15`, an out-of-range coordinate that `detect_brique` always rejects — sub-step 0
never tests anything. The code was faithful, the justification invented.

### What the double pass validated

`asm_random.c` came out **spotless**: bit-exact transcription, and its "measured"
statistics (period 4811, tail 2603, index 9 at +23.4%) reproduced by independent
simulation. Same for the 24 entries of the option table, the 120 `Blaster.inc` citations
in `constants.h` (100% right), the 16-case bounce table, the whole
`detect_prise_option` pipeline, `inc_score`/`dec_score`, the `.scr` format and its XOR
codec, and the 29 `play_sound` sites — all exact.

### Method lesson

`grep` **silently returns zero results** on these ASM files in this
environment. That is what made me write in §6c that `DELAI_INFO` was referenced
nowhere: it is (MAIN.ASM:6305) and sets the banner duration to 100 frames. Every
search now goes through `awk` and a direct read.

## 7. What this audit did not cover

`Shade_On/Off` fades (32-step palette) vs the port's alpha fades · exact z order of
`Begin_Sprites` · `display_intro` fade durations (defined in the EOS lib, outside the repo) ·
`EDITOR.ASM` and `HISCORE.ASM` visuals · GIF/LZW decoder · fidelity of the
`.iff`/`.mod` → WAV audio conversion · tracker effects (`EFFECT.ASM`, `MIXING.ASM`) ·
Android/Wear/mobile code · Android save paths · ~~behavior at 70 Hz VGA vs 60 fps (the
frame-for-frame equivalence adopted by the port was assumed, not demonstrated)~~
*(superseded — §6d demonstrated the 60 Hz WinEOS mapping from the MAKEFILE and
`Wait_Vbl`; the "70 Hz VGA" premise of this line is itself wrong)* ·
~~**no finding was confirmed by running the game**~~ *(superseded — §6c ran the port
under WSLg and confirmed the demo and the palette findings on screen)*: the verdicts of
this section come from cross-reading and from measurement on the data files.
