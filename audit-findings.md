# BrickBlaster — ASM 100% Fidelity Audit (Iter 1)

**Date**: 2026-04-18
**Rule**: HARD — nothing that isn't in the 1999 DOS ASM (`/mnt/e/dev/BrickBlaster-archive/work400/Blaster/`).
**Commit audited**: `c6b4e59` (after rollback of modern additions).

## Scope

6 parallel agents, each covering an ASM file cluster:

| Agent | Scope |
|---|---|
| F1 | MAIN.ASM 60-4712 (loop, ball, collision, paddle, game over) |
| F2 | MAIN.ASM menu/overlays/pause/demo/fade |
| F3 | MAIN.ASM powerups + monsters (24-entry struc_options) |
| F4 | HISCORE.ASM + FILE.ASM + EDITOR.ASM |
| F5 | DRAW.ASM + FONTE.ASM + MOUSE.ASM + sprite offsets |
| F6 | Blaster.inc numeric + Blaster.cfg/_en/_es byte-exact |

---

# P0 findings — 6 items (gameplay-affecting divergences)

## P0-ASM-1 — Brick score missing difficulty multipliers
- **ASM**: `FONTE.ASM:73-112 inc_score` adds +1 (med) / +2 (hard) to ecx before `ecx * 10`. Brick destroy ASM passes `ecx=1` → 10 easy / 20 med / 30 hard.
- **C**: `src/game.c:1146, 1672` hardcode `score_delta = destroyed ? 10 : 0` — same for all difficulties.
- **Fix**: extract helper `award_inc_score(g, owner, ecx)` that adds the difficulty bonus per ASM.

## P0-ASM-2 — Monster score missing difficulty multipliers
- **ASM**: `MAIN.ASM:3211-3212 mov ecx,3; call inc_score` → 30/40/50 easy/med/hard.
- **C**: `src/game.c:1867, 1915 sd = MONSTER_SCORE` (hardcoded 30).
- **Fix**: use `award_inc_score(g, owner, 3)`.

## P0-ASM-3 — BONUS/MALUS point values wrong
- **ASM**: `MAIN.ASM:6799 option_bonus_p` passes `ecx=8` (+80 base), `6817 option_malus_p` passes `ecx=12` (-120 base).
- **C**: `src/game.c:613, 616` hardcoded `+100 / -100`.
- **Fix**: `*target_score += 80` (or +80+diff_bonus); malus -120.

## P0-ASM-4 — Multi-brick-per-frame HP decrement missing
- **ASM**: `MAIN.ASM:3996 dec B [esi+ebx]` inside `detect_brique` — each corner hit inside trajectory loop decrements its own brick HP.
- **C**: `src/collision.c:420-422` calls `brick_hit` ONCE after loop exits, only on first-hit `brick_index`.
- **Fix**: move `brick_hit` call into `check_brick_at_point` when cell is breakable-normal.

## P0-ASM-5 — Dual cursor collision ignores ball ownership
- **ASM**: `MAIN.ASM:4158-4161` gate `cursor_1` on `sprite_player == player_1` in duel mode; `4281-4284` mirror for cursor_2.
- **C**: `src/game.c:1551, 1559` calls `collision_paddle` unconditionally on both paddles for every ball in MP.
- **Fix**: `if (g->game_mode == 2) { gate by ball.owner }` around each collision_paddle call.

## P0-ASM-6 — Screen shake + red flash are non-ASM additions
- **ASM**: No screen shake, no red flash. Neither is in DRAW.ASM or MAIN.ASM.
- **C**: `src/draw.c:824-829` shake, `:837-841` red flash — triggered from life loss.
- **Fix**: REMOVE per HARD RULE (no additions).

---

# P1 findings — 28 items (correctness deviations)

## Gameplay logic (F1 / F3)

- **P1-ASM-7**: GHOST spawn count. ASM `MAIN.ASM:6779-6783` spawns 20 in 1P but 10 in 2P modes. C `game.c:696` always 20.
- **P1-ASM-8**: `detect_new_life` missing +80 pts score bonus. ASM `MAIN.ASM:6463-6472` adds 8×inc_score when crossing `bonus_extra_life=10000`. C only increments lives.
- **P1-ASM-9**: Coop life-lost trigger. ASM `MAIN.ASM:4626-4629` fires `test_game_over` when in-play ball count drops to 1. C waits for 0.
- **P1-ASM-10**: REVERSE doesn't occupy global `current_option`. ASM `MAIN.ASM:5700-5714 detect_prise_option` writes it. C only sets `paddle.reverse_timer`.
- **P1-ASM-11**: Magnetic catch zeros velocity. ASM `MAIN.ASM:4207-4221` preserves vx/vy, flips sens_y only. C `src/collision.c:555-561` zeros and recomputes on launch.
- **P1-ASM-12**: MAGNETIC / GHOST per-player in ASM (via `magnetic_flag` bits PLAYER_ONE=1, PLAYER_TWO=2 at Blaster.inc:13-14; `set_ghost` filtered by current_player MAIN.ASM:3339-3358). C uses global flags.
- **P1-ASM-13**: TELEPOD 600-frame window in C vs ASM one-shot (Refresh_Ball `@@reset_current_option` MAIN.ASM:2885-2898).
- **P1-ASM-14**: `MONSTER_DELAI_*` (600/500/300) fabricated — ASM symbols never defined. Citation `FILE.ASM:1130-1132` is wrong (those lines are `delai_between_option_easy`, unrelated).

## Menu / demo / UI (F2)

- **P1-ASM-15**: Demo entry `nbs_player=1` forced. ASM `MAIN.ASM:87-90` randomises 1-or-2.
- **P1-ASM-16**: Auto-demo on idle not implemented. ASM `detect_reset_ecx` + `DELAI_DATTENTE=600` → attract demo after 10s. C counter declared but unused.
- **P1-ASM-17**: "play again" overlay not rendered. ASM `MAIN.ASM:4698` flashes `option_text_again` on life loss. C jumps to "ready" directly.
- **P1-ASM-18**: `state.demo_flag` not cleared on demo exit → leak if user then picks difficulty.

## Hiscore / file / editor (F4)

- **P1-ASM-19**: Digits 0-9 silently rejected in name entry. ASM `Get_name` accepts all `al >= 0x20`. C `screen_hiscore.c:117-122` only A-Z/a-z/space.
- **P1-ASM-20**: No BACKSPACE in name entry. ASM `ah=14` writes space+decrements edi.
- **P1-ASM-21**: `final_text` / `final_dual` victory modal screens missing (TODOs in `screen_final.c:53,66`).
- **P1-ASM-22**: Duel skips final FLC entirely in C. ASM `HISCORE.ASM:26` plays `load_final_anim` unconditionally BEFORE `dual_flag` check.
- **P1-ASM-23**: `Read_File_Config` cfg parser NOT ported. All values hardcoded. User cannot edit Blaster.cfg to tune powerup delays, lives, speeds.
- **P1-ASM-24**: `.usr` user config (volume) NOT ported. ASM `Read/Write_Config_User` persists `User_Volume` + `User_Volume_Sfx` — this IS in ASM, so should be ported per rule.
- **P1-ASM-25**: Editor missing teleporter + multi brick types (ASM F1-F5 has all 5; C only has Normal/Indestr/Transparent keys).
- **P1-ASM-26**: Editor `xchg_level` (F10 swap) and `ins_level` (Ctrl+Ins) not ported.
- **P1-ASM-27**: Final FLC plays 418 frames; ASM plays `417-33=384`. 34 trailing frames excess.

## Draw / fonte / mouse (F5)

- **P1-ASM-28**: P2 paddle sprite `vaisseau_2_o` at (0,68) NOT USED. C tints P1 paddle pink instead.
- **P1-ASM-29**: P1 right-cannon projectile `vaisseau_1_tir_right_o` at (402,183) NOT USED (both dirs render left).
- **P1-ASM-30**: P2 powerup decal row at Y=777 NOT USED (dual-mode powerups spawned during P2's ball should use this row per MAIN.ASM:5525-5533).
- **P1-ASM-31**: 5 SFX loaded but never played: `SFX_BOUNCE` (paddle hit — **user-reported missing sound**), `SFX_RESTART`, `SFX_LEVEL_COMPLETE`, `SFX_POWERUP_OFF`, `SFX_POWERUP_LOST`.

## Constants / strings (F6)

- **P1-ASM-32**: Overlay text hardcoded English — not i18n routed. FR/DE/ES/IT/PT users see English READY/PAUSED/OVER/DEMO. ASM has `option_text_*` in all language cfgs.
- **P1-ASM-33**: `GAME_OVER_DELAY_FRAMES=180` has no ASM basis. ASM uses interactive wait via `display_score`; idle-return via `DELAI_DATTENTE=600`.
- **P1-ASM-34**: `DELAI_BETWEEN_OPTION=60` ignores cfg override `Time_Between_Option (6,10,12)` per difficulty. Powerups spawn 10× less often than intended.

---

# P2/P3 findings — 20+ items (summarized)

- Hiscore background pool 1/2 vs ASM 1/4 (F4)
- Fast/slow ball rate-limited in C (F1 — intentional 60fps scaling, documented)
- BALL_3/6/9/20 powerup Y=804 vs ASM option_fade_o=802 (2px off, tuning)
- Speed counter game-wide vs per-ball (F1 — rare edge case)
- Monster destruction plays 1 SFX vs ASM 1-per-monster (F3)
- Editor keys modernized (1-4/N/I/T vs F1-F5) (F4 — may be reverted per rule)
- Credits looping: ASM loops 5 slides forever until ESC; C plays 6 once (credit_b duplicated from intro)
- Credit_B shown twice (intro + credits menu)
- FR/ES menu rewordings (I18N-1/2 — 11/28 FR and 12/28 ES divergent)
- Menu cursor static vs cursor-tracking tooltip (F2 REFRESH_TEXT-1)
- Fade hover cycle replaced by alpha overlay (documented — raylib can't do indexed palette cycling)

---

# Confirmed matches (no action needed)

- ✅ FONTE charset 45 chars byte-exact (incl. `-`)
- ✅ All panel/icon/cursor/button-rect positions match
- ✅ 86/89 Blaster.inc numeric constants byte-exact
- ✅ 28/28 EN menu labels byte-exact (trimmed)
- ✅ 4/4 implemented overlay strings byte-exact (ready/paused/over/demo)
- ✅ 28/28 menu action dispatches correct
- ✅ Hiscore struct 32 bytes, XOR codec, defaults, sentinel
- ✅ Level 390 bytes × 80, brick encoding `[CC|TTT|HHHHH]`
- ✅ Intro 36 frames × 5 ticks
- ✅ 24/24 powerup enum+freq+sprite offsets
- ✅ Per-paddle laser (SHOOT/MINI_SHOOT)
- ✅ Per-paddle size/reverse (SMALL/LARGE/REVERSE)
- ✅ Ball.owner / projectile.owner score attribution in duel
- ✅ Collision direction LUT (change_direction)
- ✅ Monster variant selection `(level-1) & 3`
- ✅ POWERUP_DEATH -1 life + DEATH.wav
- ✅ ENTER/KP_ENTER/ESC confirm in hiscore (just fixed c6b4e59)

---

# Fix plan iter 1

6 parallel agents on disjoint file clusters:

## G1 — Core scoring + collision (P0-1,2,3,4,5) → `game.c`, `collision.c`
- Extract `award_inc_score(g, owner, ecx)` helper with difficulty adds
- Wire +80 BONUS / -120 MALUS via helper
- Brick destroy = `award_inc_score(g, owner, 1)` = 10/20/30
- Monster = `award_inc_score(g, owner, 3)` = 30/40/50
- Move `brick_hit` into `check_brick_at_point` for multi-brick-per-frame
- Duel cursor collision: gate by ball.owner

## G2 — Remove non-ASM additions (P0-6) → `draw.c`, `game.c`
- Remove screen shake (`shake_timer` + `draw.c:824-829`)
- Remove red flash overlay (`draw.c:837-841`)
- Remove shake triggers in `handle_life_lost`

## G3 — Menu/demo/overlay polish (P1-15,16,17,18,32,33) → `screen_menu.c`, `main.c`, `screen_overlays.c`, `game.c`
- Demo `nbs_player` randomise
- Auto-demo after 600 idle frames
- "play again" flash on life-lost
- Clear `demo_flag` on demo exit
- Route overlay text via i18n (add STR_READY etc., use FONTE)
- Use `DELAI_DATTENTE=600` for game-over or make it interactive

## G4 — Hiscore + editor (P1-19,20,21,22,25,26) → `screen_hiscore.c`, `screen_final.c`, `screen_editor.c`, `main.c`
- Digits 0-9 + BACKSPACE in name entry
- Add `final_text` modal (post-anim before hiscore)
- Add `final_dual` modal for duel
- Duel: play final FLC too
- Editor: teleporter + multi keys
- Editor: xchg_level F10 / ins_level Ctrl+Ins

## G5 — Sprites + audio (P1-28,29,30,31) → `draw.c`, `audio.c`, `game.c`
- Wire `vaisseau_2_o` (0,68) for P2 paddle
- Wire `vaisseau_1_tir_right_o` (402,183) for P1 right cannon
- Wire `vaisseau_2_tir_left/right/big` (374/402/446 at Y=405/435)
- Wire option_2_decal Y=777 for P2 powerups in dual
- Trigger SFX_BOUNCE on paddle hit (MAIN.ASM:4217, 4340)
- Trigger SFX_LEVEL_COMPLETE on level advance (MAIN.ASM:4141)
- Trigger SFX_RESTART on STATE_READY_TO_PLAY_AGAIN (MAIN.ASM:260)
- Trigger SFX_POWERUP_OFF on timer expiry (MAIN.ASM:6322)
- Trigger SFX_POWERUP_LOST when powerup falls off (MAIN.ASM:5592)

## G6 — Per-player powerup state + config loader (P1-12,13,23,24,34) → `game.c`, `powerup.c`, new `settings.c/h`
- Magnetic: split into `paddle.magnetic_active` or `g->magnetic_flag` bitmask
- Ghost: filter `set_ghost`-equivalent by collector
- TELEPOD: one-shot instead of 600-frame window
- Port `Read_File_Config` parser for Blaster.cfg override values
- Port `.usr` volume save (this IS in ASM)
- DELAI_BETWEEN_OPTION per-difficulty (6/10/12)

Each agent builds after its batch. Final cumulative build must be clean. Commit per group.
