# BrickBlaster C Port — Audit Findings

**Audit date**: 2026-04-17
**Audit method**: automated ASM cross-reference, parallel agents, max effort
**Scope**: 6 parallel batch audits (core gameplay, powerup, game systems, UI, I/O, MP) + 3 exploration agents
**Commit audited**: `afbd060` (main branch, standalone repo)
**ASM reference**: `/mnt/e/dev/BrickBlaster-archive/work400/Blaster/` (1999 MS-DOS original)

## Severity scale
- **P0** : Gameplay-affecting bug (score wrong, collision wrong, mode unplayable)
- **P1** : Correctness deviation from ASM, game still playable
- **P2** : Visual/UX deviation
- **P3** : Code quality / docs / dead code

## Raw counts (pre-dedup)

| Batch | P0 | P1 | P2 | P3 | Total |
|---|---|---|---|---|---|
| A — Core gameplay | 6 | 4 | 2 | 2 | 14 |
| B — Powerup system | 3 | 1 | 3 | 2 | 9 |
| C — Game systems | 1 | 5 | 6 | 4 | 16 |
| D — UI & screens | 1 | 5 | 7 | 4 | 17 |
| E — I/O & assets | 0 | 4 | 6 | 4 | 14 |
| F — MP correctness | 3 | 6 | 3 | 3 | 15 |
| **Raw total** | **14** | **25** | **27** | **19** | **85** |

## Deduplicated counts

| Severity | Count | After dedup |
|---|---|---|
| P0 | 12 | gameplay blockers |
| P1 | 18 | correctness deviations |
| P2 | 22 | visual/UX |
| P3 | 15 | code quality |
| **Total** | **67** | |

## Mode verdicts

| Mode | Before fixes | After P0 fixes |
|---|---|---|
| Solo (mode 0) | ✅ shippable | ✅ |
| Coop (mode 1) | 🔴 NOT shippable (lives bug) | ✅ if P0-1,5,6 fixed |
| Duel (mode 2) | 🔴 NOT shippable | ✅ if P0-1 through P0-12 fixed |

---

# P0 findings (gameplay blockers) — 12 items

## P0-1 — Ball-lost decrements BOTH players' lives unconditionally
**Root cause of most MP breakage.**

- **C site**: `src/game.c:843-844` `g->lives--; if (g->game_mode > 0) g->lives_2--;`
- **ASM ref**: MAIN.ASM:4595 `detect_game_over_player_1` (per-player path)
- **Modes**: coop, duel
- **Description**: Every ball lost decrements BOTH counters. In duel, P1's lost ball costs P2 a life. In coop, every death is double-counted.
- **Fix**: Route through `ball.owner`. Pass lost ball into `handle_life_lost`, decrement only `lives` or `lives_2` based on owner. Game-over logic at L846-851 already has right per-player check.
- **Found by**: Batch A-F4, F-F1,F3

## P0-2 — Duel game-over OR-logic wrong
- **C site**: `src/game.c:851` `game_over = p1_dead || p2_dead;`
- **ASM ref**: MAIN.ASM:4595-4712
- **Mode**: duel
- **Description**: "First out ends match" — likely wrong. ASM disables losing paddle but keeps game until BOTH exhausted. Combined with P0-1 makes duel unplayable (first ball lost → both dead → game over).
- **Fix**: Keep AND for duel; fix P0-1 first to decouple lives counters; verify ASM MAIN.ASM:4595-4712 reference.
- **Found by**: Batch A-F4, F-F2,F3

## P0-3 — Magnetic ball launch & tracking ignore P2 paddle
- **C site**: `src/game.c:1307-1311` (launch) + `1320-1336` (follow)
- **ASM ref**: MAIN.ASM:5246-5280 (detect_start_game), MAIN.ASM:4200-4204
- **Mode**: duel
- **Description**: Magnetic ball launch uses `g->paddle.x + paddle.w/2`. Follow code pins ball Y to `g->paddle.y`. P2 magnetic ball launches from P1 paddle, follows P1 paddle. Also only `input->fire_pressed` honored; `input->p2_fire` can't launch P2's ball.
- **Fix**: Use `owner_paddle = (ball.owner == 1) ? &g->paddle_2 : &g->paddle`. Honor both fire inputs per owner.
- **Found by**: Batch A-F5

## P0-4 — LARGE_SHIP / SMALL_SHIP / REVERSE only affect P1
- **C sites**:
  - `src/game.c:539-543` conflict-clear targets `g->paddle`
  - `src/game.c:707-724` `paddle_set_size(&g->paddle, ...)` hardcoded
  - `src/game.c:719-724` `g->paddle.reversed = 1` hardcoded
  - `src/game.c:467, 471, 475` deactivate uses `g->paddle`
- **ASM ref**: MAIN.ASM:6491 (option_small_ship_p), 6500 (option_large_ship_p), 6562 (option_reverse_p)
- **Mode**: duel
- **Description**: P2 collects LARGE_SHIP → P1's paddle enlarges, P2 stays normal. Same for SMALL_SHIP and REVERSE. Per-paddle state exists (already used for laser) but not wired here.
- **Fix**: Use `owner_paddle` (already computed at L526). Add per-paddle flags `small_paddle_active_2`, `large_paddle_active_2`, `reverse_active_2` if global ones must remain. Follow laser-per-paddle pattern.
- **Found by**: Batch A-F6, B-F4, F-F5

## P0-5 — POWERUP_BONUS / POWERUP_MALUS credit wrong player in duel
- **C site**: `src/game.c:592-598`
- **ASM ref**: MAIN.ASM:6802 (option_bonus_p), 6816 (option_malus_p), 4024-4026 (inc_score dispatch on sprite_player)
- **Mode**: duel
- **Description**: `g->score += 100` / `g->score -= 100` always on P1. P2 collects bonus → P1 gets +100.
- **Fix**: Use `collected_by` owner : `int *target = (game_mode == 2 && collected_by == 1) ? &score_2 : &score;`
- **Found by**: Batch A-F1, B-F1, F-F6

## P0-6 — POWERUP_NEW_LIFE always extra life to P1
- **C site**: `src/game.c:582-586`
- **ASM ref**: MAIN.ASM:6426 (option_new_life_p), 6436-6439 clamp
- **Mode**: duel
- **Description**: `g->lives++` always targets P1's life counter.
- **Fix**: Same dispatch pattern as P0-5. Also handle `bonus_life_threshold_2` if relevant.
- **Found by**: Batch A-F2, B-F2, F-F6

## P0-7 — Extra balls (3/6/9/20) + GHOST spawn from P1 paddle, owner=0
- **C sites**: `src/game.c:500-510` (spawn_extra_balls), `game.c:681-692` (POWERUP_GHOST)
- **ASM ref**: MAIN.ASM:option_3_ball_p, option_ghost_p (active player's cursor)
- **Mode**: duel, coop (cosmetic)
- **Description**: P2 collects multi-ball → balls spawn from P1 paddle, owner=0 default → cheap score gain for P1 on P2's collection.
- **Fix**: Pass `collected_by`. Spawn at `owner_paddle.x`. Set `ball.owner = collected_by`.
- **Found by**: Batch F-F7

## P0-8 — Transparent brick destroy score always to P1
- **C site**: `src/game.c:1691-1695` sync loop after damage
- **ASM ref**: MAIN.ASM:4024-4026 (sprite_player dispatch)
- **Mode**: duel
- **Description**: Transparent brick destroy detected in deferred sync loop with no owner context. `g->score += sd` credits P1 always. Separate from the TODO at game.c:1653 (partial impl).
- **Fix**: Track owner at damage time — either plumb owner through `check_brick_at_point` or stamp brick with owner at time of hit, read in sync loop.
- **Found by**: Batch A-F3, F-F15

## P0-9 — Menu 6 has Editor instead of Hiscore-Coop (QW4 resolved)
- **C site**: `src/screen_menu.c:70` `M6[4] = {STR_M_HISCORES, STR_M_EDITOR, STR_M_CREDITS, STR_M_CANCEL}`
- **ASM ref**: MAIN.ASM:6913-6941 via `constants.h:410-413` `M_SCORE_1=21, M_SCORE_2=22, M_CREDIT=23, M_CANCEL_5=24`
- **Mode**: all
- **Description**: Slot 6/b in ASM is Hiscore-Coop. `STR_M_HISCORES_COOP` exists in i18n.c:83-85 but unused. Editor was not in menu 6 originally (it's reachable via F-key / separate entry). Coop hiscore table unreachable from menu.
- **Fix**: Change M6[1] to `STR_M_HISCORES_COOP`. Action handler sets `state->hiscore_mode = 1; state->game_mode = STATE_HISCORE`. Move Editor to debug-only key or separate menu 7 slot. `state->hiscore_mode` already supported.
- **Found by**: Batch D-F1 (Explore-agent QW4 pre-audit)

## P0-10 — Hiscore qualifier locks out first-time players
- **C site**: `src/hiscore.c:200-205` `hiscore_qualifies` checks only last entry
- **ASM ref**: HISCORE.ASM:216-274 `detect_new_score` (scan loop, first-match `jae`)
- **Mode**: all
- **Description**: Default table values 1..15 (hiscore.c:75 `en->value = e+1`). Qualifier checks `score >= entries[14].value` (value=15). First-time players with score <15 never prompt for name. ASM scans from top, qualifies on first `score >= entries[i].value`.
- **Fix**: Replace with scan loop: `for i in 0..14: if score >= entries[mode][i].value return 1; return 0;`. Or inline via `hiscore_insert() != -1`.
- **Found by**: Batch C-F4,F17

## P0-11 — Dual mode writes to coop hiscore table
- **C site**: `src/main.c:491` `hs_mode = (nbs_player > 1) ? 1 : 0`
- **ASM ref**: HISCORE.ASM:28-29 (dual_flag skip victory)
- **Mode**: duel
- **Description**: Duel scores contaminate coop leaderboard. Dual should skip hiscore entry per ASM (already TODO'd at screen_final.c:40).
- **Fix**: `if (game.game_mode == 2) { state.game_mode = STATE_MENU; break; }` in main.c:486-511. Also address screen_final.c:40 TODO (skip animation in dual, or add intermediate text).
- **Found by**: Batch C-F3, F-F4

## P0-12 — No AI for P2 "computer" control (QW1 resolved)
- **C site**: `src/input_frame.c:201` comment lies; `src/game.c:1240-1247` has no AI dispatch
- **ASM ref**: MAIN.ASM:5131-5144 (demo_move_x_player_1, implicit P2 twin)
- **Mode**: coop, duel (when control_p2==0)
- **Description**: Comment says "game.c handles AI paddle_2" — false. When `control_p2 == 0` (computer), P2 paddle frozen. Coop/duel with computer P2 = unplayable.
- **Fix**: Port `demo_move_x_player_1` equivalent for P2. Add `demo_update_paddle_2(Game*)` that tracks the P2-owned ball (or nearest). Call when `control_p2 == 0 && game_mode > 0`.
- **Found by**: Batch C-F2, E-F1, F-F9 (Explore-agent QW1 pre-audit)

---

# P1 findings (correctness deviations) — 18 items

## P1-1 — Life-lost clears ALL powerups including P2's laser
- **Site**: `src/game.c:825-840` `handle_life_lost`
- **ASM**: MAIN.ASM:4669-4672
- **Fix**: Tie to owner of lost ball; only reset that paddle's gun state.

## P1-2 — POWERUP_IRON_BALL duration spec/impl contradiction
- **Site**: `src/powerup.c:317` returns 600 frames, `src/game.c:647-656` treats as permanent
- **ASM**: MAIN.ASM:6453 option_iron_ball_p (needs verification)
- **Fix**: Verify ASM; either make timed (set current_option_count) OR return -1/0 from powerup_duration. Update both in sync.

## P1-3 — Demo entry hardcodes 1 player (ASM randomises 1 or 2)
- **Site**: `src/screen_menu.c:138-141` sets `state->nbs_player = 1`
- **ASM**: MAIN.ASM:85-105 `get_random(1..2); inc; mov nbs_player`
- **Fix**: `state->nbs_player = GetRandomValue(1, 2); state->dual_flag = 0;`

## P1-4 — No P2 demo AI
- **Site**: `src/demo.c:43-70` P1 only
- **ASM**: MAIN.ASM:5131-5144 P1, P2 twin exists
- **Fix**: Add `demo_update_paddle_2`; call both from `game_update` when demo_active && game_mode>0.

## P1-5 — GAME_OVER_DELAY_FRAMES = 180 (3s), unclear if ASM matches
- **Site**: `src/screen_overlays.h:23`
- **ASM**: MAIN.ASM:4666 wait delai constant (needs reading)
- **Fix**: Verify ASM wait constant; reconcile to 60 if original was ~1s.

## P1-6 — Idle→demo (DELAI_DATTENTE=600) not implemented
- **Site**: `src/screen_menu.c:32,255-256` — `IDLE_TO_DEMO` declared but unused
- **ASM**: Blaster.inc:415 DELAI_DATTENTE=600, MAIN.ASM:221+ demo_on path
- **Fix**: When `idle_frames >= IDLE_TO_DEMO`, trigger same action as menu-1/demo button.

## P1-7 — STATE_FINAL TODOs unresolved
- **Sites**: `src/screen_final.c:40,44,57`
- **ASM**: HISCORE.ASM:28-29 (dual_flag skip), HISCORE.ASM:43-53 (intermediate victory text)
- **Fix**: Skip animation if `state->dual_flag`. Add STATE_VICTORY_TEXT display ~5s before STATE_HISCORE.

## P1-8 — Editor save path scheme invented
- **Site**: `src/screen_editor.c:74-75` `data/custom_%d.lv%d`
- **ASM**: FILE.ASM:1195-1196 `.lv?` + world char (overwrites)
- **Fix**: Either adopt ASM overwrite (warn + allow) or fallback-read custom path in game_load_level. Init `ed->difficulty_idx` from `state.difficulte`.

## P1-9 — Overlay text bypasses i18n; hardcoded lowercase English
- **Sites**: `src/screen_overlays.c:33,39,46,60` + dead `STR_READY/GAME_PAUSED/GAME_OVER/DEMO_LABEL` in i18n.c
- **ASM**: MAIN.ASM:965, 1261, 4681, 6997 (option_text_* from Blaster.cfg)
- **Fix**: Lookup i18n (add lowercase variants matching FONTE charset); pad to 18 chars centred. OR remove dead keys, document overlay-EN-only. Also QW2 case mismatch addressed.

## P1-10 — DE/IT/PT missing ~31 STR_M_* menu labels
- **Site**: `src/i18n.c:177-398` DE/IT/PT blocks incomplete
- **Fix**: Add missing menu_text entries, OR drop languages if scope-deferred.

## P1-11 — Dead duplicate music system in audio.c
- **Site**: `src/audio.c:120-166` (`audio_play_music/_stop/_update`), `src/audio.h:59-89`
- **Description**: Full `Music` API implemented but never called. `music_manager.c` is the real system.
- **Fix**: Delete dead code, keep audio.c strictly SFX.

## P1-12 — hiscore_qualifies off-by-one
- **Site**: `src/hiscore.c:200-205`
- **Already covered by P0-10**. Listing here for reference.

## P1-13 — XOR codec field offsets not statically asserted
- **Site**: `src/hiscore.c:45-53`
- **Fix**: Add `_Static_assert(offsetof(HiscoreEntry, score_text) == 4)` etc.

## P1-14 — Menu 4 second world label (resolved)
- **Site**: `src/screen_menu.c:66`, `i18n.c`
- **ASM**: `Blaster.cfg` / `Blaster_en.cfg` / `Blaster_es.cfg` all use
  `menu_text_4b ("arcade")` — the second world is "arcade", NOT "eclipse"
  (Eclipse is the *demomaker team* name, not a world name).
- **Status**: fixed — code uses `STR_M_ARCADE`. Dead `STR_SPACE` /
  `STR_ECLIPSE` enum entries and their 6-language translations were
  removed.

## P1-15 — World 2 exists (Blaster.lv2 31200 bytes) but unreachable from menu
- **Site**: `src/screen_menu.c:166-172` world menu only exposes 0, 1
- **Fix**: Add third world button OR document as future use.

## P1-16 — Speed multiplier tables not ASM-derived
- **Site**: `src/input_frame.c:25-26`
- **Description**: 5-level tables are post-ASM enhancement (ASM had scalar `speed_counter=6`). Intentional; document.
- **Fix**: Add comment "post-ASM enhancement; multiplies base".

## P1-17 — Projectile owner never bulk-reset
- **Site**: `src/game.c:836` area
- **Description**: `proj_count = 0` is the active-count; per-slot `.owner` may retain stale values. Safe today if count gates visibility.
- **Fix**: Audit visibility path; optionally add reset loop. **Minor** — promote from QW7.

## P1-18 — Monster top-bounce flip conditional not unconditional
- **Site**: `src/monster.c:132-141`
- **ASM**: MAIN.ASM:3109-3122 `force sens_y = +1`
- **Fix**: `monsters[i].vy = (monsters[i].vy == 0) ? 1 : abs(monsters[i].vy);`

---

# P2 findings (visual/UX) — 22 items

Summarized; see per-batch reports for detail.

- **P2-1 — Missing SFX triggers (13/20 declared, never fired)**. Most impactful: SFX_BOUNCE, SFX_NEW_LIFE, SFX_LEVEL_COMPLETE, SFX_GAME_OVER, SFX_LARGE/SMALL_PADDLE, SFX_DEATH_POWERUP. Batch E-F5.
- **P2-2 — Projectile sprite mismatch**: SR_PROJ_MINI (3×13) / SR_PROJ_BIG (8×42) use generic `mini_shoot_o`/`shoot_o`; collision box is 12×18 / 13×19. Art/hitbox mismatch. Fix: re-source to `vaisseau_1_tir_*` at (374,183)/(446,413). Batch E-F6, Initial D.
- **P2-3 — P2 paddle uses P1 sprite tinted pink**. ASM has `vaisseau_2_o` at Blaster.inc:226. Batch E-F8, F-F14.
- **P2-4 — P2 FIRE hint mobile (QW6)**: `main.c:413-415` ignores `paddle_2.has_gun`. Batch F-F11.
- **P2-5 — P2 projectile sprite**: even if P2-8 fixed, needs P2 variant at (374,405)/(446,435). Batch F-F13.
- **P2-6 — Monster explosion offset**: `monster.c:168-169` uses `/2=19` instead of ASM constant 16. Batch C-F7.
- **P2-7 — Monster spawn X range inclusive upper bound**. Batch C-F8.
- **P2-8 — FLC timing uses fixed 1/60 instead of GetFrameTime**. Intentional workaround. Batch C-F10.
- **P2-9 — Demo exit doesn't reset multi-ball state**. Brittle. Batch A-unknown, C-F16.
- **P2-10 — Ghost mode wraps balls instead of "pass through"**. Semantics unclear. Batch A-F11.
- **P2-11 — Teleporter cooldown allows 1-frame ball_move post-teleport**. Cosmetic. Batch A-F12.
- **P2-12 — Hover pulse uses wall-clock not frame counter**. Drops skip phase. Batch D-F10.
- **P2-13 — Hiscore column layout C-invented (no ASM cite)**. Batch D-F11.
- **P2-14 — Blinking cursor 6 Hz too fast vs DOS ~2 Hz**. Batch D-F12.
- **P2-15 — Hiscore punctuation remaps to 'A' silently**. Batch D-F13.
- **P2-16 — Credits timing: 300 frames (constants.h) vs 160 (screen_credits.h)**. Reconcile. Batch D-F9.
- **P2-17 — Editor missing on-screen panel (Blaster.inc:357-360 arrow sprites)**. Batch D-F15.
- **P2-18 — Cursor anchor vs centre** — verify Blaster.inc cursor_menu_o. Batch D-F16.
- **P2-19 — Break-anim comment says blue but samples orange**. Batch E-F7.
- **P2-20 — Duplicate background folder wastes disk on mobile**. Batch E-F9.
- **P2-21 — STATE_TITLE dead state**. Batch D-F7.
- **P2-22 — "draw" overlay text has 7 leading spaces, off-centre**. Batch E-F12.

---

# P3 findings (code quality) — 15 items

- **P3-1 — Score-award logic duplicated in 5 places** (projectile-brick, ball-brick, monster-ball, monster-projectile, transparent-brick). Batch A-F13. Extract helper `award_score(g, owner, delta)`.
- **P3-2 — Collision LUT FACE_X/Y encoding undocumented**. Batch A-F14.
- **P3-3 — `deactivate_current_option` conflict-clear redundant with switch**. Batch B-F8.
- **P3-4 — POWERUP_DEATH is intentional no-op but still drops**. Batch B-F7. Remove from spawn pool or restore.
- **P3-5 — `dev_next_powerup` advances cycle on every call**. Batch B-F9.
- **P3-6 — `hiscore_qualifies` off-by-one flagged twice (P0-10)**.
- **P3-7 — TakoHi slide shown twice on non-Android** (screen_intro.c + screen_intro_original.c end). Batch D-F14.
- **P3-8 — Editor has no on-screen panel matching EDITOR.ASM**. Batch D-F15.
- **P3-9 — Menu cursor anchor centre vs top-left**. Batch D-F16.
- **P3-10 — screen_update_music misses STATE_EDIT/PAUSED/GAME_OVER/NEW_PLAY**. Batch D-F8.
- **P3-11 — audio.h comment "MONSTOFF.wav (closest available)"** suggests asset gap. Batch E-F11.
- **P3-12 — `music_manager.c` uses `ctxData != NULL` instead of `frameCount > 0`**. Batch E-F14.
- **P3-13 — `level_load` doesn't distinguish file-missing vs size-wrong**. Batch C-F14.
- **P3-14 — Custom editor path never read back in normal flow**. Batch C-F15.
- **P3-15 — `demo_handle_ball_lost` doesn't clear balls[1..]**. Batch C-F16.

---

# Quick-wins (Phase 1 pre-audit)

From Explore-phase findings. Most promoted to P0 or P1 above.

| QW | Promoted to | Status |
|---|---|---|
| QW1 — Missing P2 AI | P0-12 | needs fix |
| QW2 — Overlay case mismatch | P1-9 | needs fix |
| QW3 — FLI frame count 418 vs 384 | resolved (Batch C-F11 confirmed 418 correct; update planning doc only) |
| QW4 — Menu 6 Editor vs Hiscore-Coop | P0-9 | needs fix |
| QW5 — Transparent brick destroy | P0-8 | needs fix |
| QW6 — P2 FIRE hint mobile | P2-4 | minor |
| QW7 — Projectile owner bulk reset | P1-17 | minor |

---

# Cross-cutting pattern: Per-owner dispatch

**Nearly every P0 falls into this pattern**: a site that touches game state uses `g->paddle` / `g->score` / `g->lives` / `g->*_active` hardcoded for P1 instead of routing via `owner` / `collected_by` / `ball.owner`.

The laser (SHOOT / MINI_SHOOT) was correctly migrated in commit `d7d06a0`. The fix pattern for all remaining P0s follows the same template :

```c
Paddle *owner_paddle = (g->game_mode > 0 && owner == 1) ? &g->paddle_2 : &g->paddle;
int *target_score = (g->game_mode == 2 && owner == 1) ? &g->score_2 : &g->score;
int *target_lives = (g->game_mode == 2 && owner == 1) ? &g->lives_2 : &g->lives;
```

The `game_mode == 2` gate ensures coop still pools correctly (coop uses `g->score` and `g->lives` shared).

---

# Phase 4 — Fix plan

## Priority order
1. **P0-1** (ball-lost per-owner lives) — unblocks coop + duel single-handedly.
2. **P0-5, P0-6** (BONUS/MALUS/NEW_LIFE per-owner) — trivial dispatch fix.
3. **P0-3, P0-4, P0-7** (per-owner powerup effects: magnetic, ship-size, reverse, multiball) — same owner pattern.
4. **P0-8, P0-10, P0-11** (transparent brick score, hiscore qualifier, dual hiscore table) — hiscore-adjacent fixes.
5. **P0-9** (menu 6 Editor → Hiscore-Coop) — string swap + action dispatch.
6. **P0-2** (duel game-over OR/AND) — verify after P0-1 fix.
7. **P0-12** (P2 AI) — port `demo_update_paddle_2`.

## Grouping for parallel agents
- **Agent G1 — MP ownership dispatch**: P0-1, P0-5, P0-6, P0-7, P0-8. All `game.c` same-file.
- **Agent G2 — Paddle state per-player**: P0-3 (magnetic), P0-4 (size/reverse). `game.c` + `paddle.h`.
- **Agent G3 — Menu + hiscore**: P0-9, P0-10, P0-11. `screen_menu.c`, `hiscore.c`, `main.c`.
- **Agent G4 — P2 AI**: P0-12. `game.c` + `demo.c`.
- **Post-fix review**: P0-2 re-verify.

**P1 through P3 are backlog** — not bundled with this audit's fix phase. User decides case-by-case.

---

# Known exclusions from this audit

- ASM files not directly readable by sandbox agents in some batches — cited references rely on existing C comments + headers. Where uncertainty exists, flagged with "ASM needs verification".
- Mobile-specific code (input_tilt, input_wear, mobile_controls) — audited only at integration boundary, not internals.
- Performance profiling — out of scope.
- Playtesting — Phase 5 responsibility.
