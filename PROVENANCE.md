# Provenance and licensing record

This document is a factual record of where the material in this repository
comes from and what the available sources state about its licensing. It is
**not a legal opinion**. No lawyer has reviewed this repository or this
document. Where something is unknown, it is recorded as unknown, with the
question that would need to be answered by the upstream author. Statements
below describe what sources *say*, with citations; they do not assert what
is or is not permitted.

All online sources were consulted on 2026-08-16. File comparisons were made
against a local clone of `david4599/BrickBlaster` (remote
`https://github.com/david4599/BrickBlaster.git`, branch `main`, clean
working tree, HEAD `aa04971`), whose `work400/` tree contains the 1999
source material referenced below.

## 1. The license chain

**1999.** Brick Blaster was a commercial MS-DOS/Windows game published on
the *Media Pocket* label in 1999, written by members of the French
demoscene group Eclipse. The 1999 source tree preserved in
`david4599/BrickBlaster` (`work400/`) contains **no license file and no
copyright notice**: none of the `.ASM` source files carries a copyright
banner (checked by grep across `work400/Blaster/*.ASM`), and no license
text ships with the 1999 assets. Whatever terms governed the 1999
commercial release are not present in any repository examined here.

**2024.** On 2024-03-10 (repository `created_at` per the GitHub API),
GitHub user **david4599** published two repositories, both carrying a
`LICENSE` file containing the verbatim GNU GPL v3 text and both detected
by GitHub as GPL-3.0:

- `https://github.com/david4599/BrickBlaster` — description: "Brick
  Blaster game source code (Media Pocket 1999)". README: "This repository
  contains the source code of the game Brick Blaster (Media Pocket 1999)
  including WinEOS 4.00 Alpha and instructions on how to build it."
- `https://github.com/david4599/BrickBlaster-EOS-Archive` — description:
  "Archive of Brick Blaster game source code (Media Pocket 1999)". Holds
  the unmodified source snapshot, 1999 binaries, WinEOS 4.00 Alpha, and
  EOS 3.05/3.06.

The only statement in either repository about how david4599 obtained the
material is the acknowledgment in both READMEs:

> "A big thanks to the author Marc Radermacher
> ([website](https://www.edromel.com)) for kindly providing the source
> code of this awesome game and allowing to preserve it!"

That sentence is the entire documented grant. Neither repository contains
a sentence of the form "released under GPL v3 with the permission of the
author", names a copyright holder, or fills in the GPL's "How to Apply
These Terms" appendix (the `<year> <name of author>` placeholders in the
LICENSE file are the unmodified FSF template). Both repositories have zero
issues (open or closed), so no clarification exists in an issue tracker
either.

**2026 (this port).** This repository's `LICENSE` is byte-identical to the
upstream `LICENSE` (verified with `diff`). The README states: "GNU General
Public License v3.0 — inherited from the original BrickBlaster sources."
Note the wording: the 1999 sources themselves carry no license text; the
GPL v3 text first appears in the 2024 repositories. Who chose GPL v3 —
Marc Radermacher or david4599 — is not stated anywhere that was found.

## 2. Whether the assets are covered — the central unknown

The GPL v3 is written primarily for software ("The GNU General Public
License is a free, copyleft license for software and other kinds of
works" — LICENSE, Preamble; "'The Program' refers to any copyrightable
work licensed under this License" — §0). The upstream repositories
include, in the same tree as the code, the game's data: images
(`*.GIF`), palettes (`Sprite0.pal`, `Sprite1.pal`), music modules
(`*.mod`), sound samples (`*.IFF`), FLC videos, level files
(`Blaster.lv0/.lv1/blaster.lv2`), and the localized text in
`Blaster*.cfg`.

**No statement was found, anywhere, that addresses the assets
specifically.** Checked: both upstream LICENSE files, both READMEs, both
repository descriptions, both issue trackers (zero issues each), and the
upstream commit history visible in the local clone. The READMEs discuss
the assets only technically (which players open `.IFF`/`.MOD`/FLC files).
The acknowledgment sentence quoted above says Marc Radermacher provided
"the source code"; it does not mention the graphics (credited upstream to
Frédéric Box), the music (credited upstream to Christophe Résigné), or
the levels.

This is the principal open question of this document (see §7). This
repository redistributes conversions of those assets on the working
assumption that the upstream GPL v3 publication covers the whole tree; that
assumption is exactly what the questions in §7 ask upstream to confirm or
correct.

## 3. Provenance inventory of this repository

Established by direct file comparison and inspection on 2026-08-16, on
branch `fix/asm-parity-2026-08`.

### 3.1 Material derived from the 1999 originals

Byte-identical copies (verified with `cmp` against
`work400/Blaster/`):

- `assets/levels/Blaster.lv0`, `assets/levels/Blaster.lv1`,
  `assets/levels/blaster.lv2` — identical to the 1999 level files.
- `assets/blaster.ico` — identical to the 1999 `blaster.ico`.
- `data/blaster.cfg` — identical to the 1999 `Blaster_en.cfg` (the
  English localization shipped upstream).

Format-preserving conversions (original file format → repository format;
the conversion mapping is one-to-one by filename):

- `assets/audio/*.wav` — 17 files, converted from the 17
  `work400/Blaster/resource/*.IFF` samples of the same names
  (BOOM, BOUNCE, DEATH, ENDOPT, LARGE, MONSTOFF, NEWLIFE, NEXT, NIGHT,
  OPTIONON, PERTEBAL, RESTART, SHOOT, SMALL, SPEEDUP, TELEPOD, WALL).
- `assets/music/*.wav` — 5 files, converted one-to-one from the 5
  `resource/*.mod` tracker modules of the same names (Blaster, Credit,
  Lode, Rain, Thelast), matching the five paths listed in
  `src/music_manager.c:8-12`. The web build additionally transcodes these
  to OGG Vorbis at package time (`.github/workflows/web.yml`), which is why
  `music_manager.c` prefers a `.ogg` sibling when one exists.
- `assets/sprites/` — PNG renderings of the 1999 indexed GIFs:
  `SPRITE.png` and `SPRITE0.png` are `resource/SPRITE.GIF` rendered
  through `Sprite1.pal` and `Sprite0.pal` respectively (the mechanism is
  documented in `src/assets.c:57-60`, mirroring `FILE.ASM:776-791`
  `Read_Palette`); `FONTE.png` ← `FONTE.GIF`; `MONSTER.png` ←
  `MONSTER.GIF`; `MENU.png` ← `MENU.GIF`; `Blaster.png` ←
  `resource/Blaster.gif`; `CREDIT_B/C/E/G/M.png`, `Credit_w.png` ←
  the GIFs of the same names; `00_01.png`…`01_08.png` ← the sixteen
  background GIFs `resource/00_01.gif`…`01_08.gif`.
- `assets/backgrounds/00_01.png`…`01_08.png` — the same sixteen 1999
  background GIFs converted to PNG. The world-1 set was regenerated in
  August 2026 through the correct palette (`Sprite1.pal`) after the
  parity audit found the initial conversion had used the wrong one
  (see `audit-2026-08-13-parity.md` and the README fidelity section).
- `assets/credits/credit_*.png` — 6 files ← `resource/CREDIT_*.GIF`.
- `assets/intro/frame_0001.png`…`frame_0036.png` — 36 frames extracted
  from `resource/Intro.flc`.
- `assets/final/frame_0001.png`…`frame_0418.png` — 418 frames extracted
  from `work400/Blaster/blaster.flc`.
- `assets/title/media.png` ← `work400/Blaster/media.gif` (Media Pocket
  splash); `assets/title/blaster.png`, `assets/title/menu.png`,
  `assets/menu/blaster.png`, `assets/menu/menu.png` ← `Blaster.gif` /
  `MENU.GIF`.
- `assets/blaster_icon.png` — 64×64 PNG of the original game icon (see
  `CHANGELOG.md`, entry on restoring the original icon).
- `data/blaster.usr` — 2-byte volume file in the 1999 `FILE.ASM:813`
  format (values are runtime state, not a copy: local file is
  `0x40 0x40`, the 1999 `Blaster.usr` is `0x22 0x40`).
- `data/blaster.scr` — high-score file in the 1999 XOR-encoded format;
  generated at runtime and **not tracked in git** (`git ls-files data/`
  lists only `blaster.cfg` and `blaster.usr`).

Not carried over: `HELP.GIF`, `ARCADE.GIF`, `Blaster.cfg` (French),
`Blaster_es.cfg`, `media_bak.gif`, `Blaster_bak.scr`.

The C source in `src/` is itself a derivative of the 1999 `*.ASM` sources
in the translation sense: the README states the gameplay internals are
"ported byte-for-byte with ASM line citations", and the source files cite
`MAIN.ASM` / `FILE.ASM` line numbers throughout.

### 3.2 Material written for the port (2026)

Everything under `src/` (C99 sources and headers, `brickblaster.rc`),
`tests/test_parity.c`, `.github/workflows/` (`build.yml`, `web.yml`,
`android.yml`), `web/shell.html`, the `android/` Gradle project,
`CMakeLists.txt`, `CHANGELOG.md`, `README.md`, and the three audit
documents (`audit-2026-08-13-parity.md`, `audit-findings.md`,
`audit-asm-faithful.md`). Author: Jonathan Odul (KonsomeJona), per the
README credits and the git history. None of these files carries a
per-file GPL header; the repository relies on the top-level `LICENSE`
(the same is true of the upstream ASM files).

### 3.3 Material added by the port's publisher

`assets/takohi/` — `Icon.ico`, `Icon.png`, `Logo_Eng.png`,
`Logo_Square_Eng.png`, `blaster_original.png`. The README labels this
directory "Publisher branding (stripped by `-DTAKOHI_BRANDING=OFF`)".
These files are present since the initial commit (`7989e51`) and are used
only when the `TAKOHI_BRANDING` CMake option is ON (default ON; the
README documents `-DTAKOHI_BRANDING=OFF` for "a 100%-vanilla build").
`blaster_original.png` is a 64×64 PNG of the original 1999 game icon, so
it belongs to family 3.1 despite its location. **The repository contains
no statement of who owns the TakoHi name and logo and no separate license
notice for these files**; they sit in the tree under the repository-wide
`LICENSE` with no stated exception. Whether that is the intent of the
TakoHi rights holder is a fact only the port publisher can state (§7).

### 3.4 Third-party components

- `third_party/gif.h` — header-only GIF encoder by Charlie Tangora
  (`github.com/charlietangora/gif-h`); its header states "Public domain."
- raylib 5.0 — fetched at build time via CMake `FetchContent`, not
  vendored; zlib-licensed per raylib's own repository.

## 4. GPL v3 obligations — state of the record

Factual observations only; whether they amount to compliance is not
assessed here.

- **License text (§4):** present — `LICENSE` at the repository root,
  byte-identical to the upstream copy of the GPL v3 text.
- **Modification notices (§5a/§5b):** present — the README carries a
  section titled "Modifications from the 1999 original (GPL §5 notice)"
  listing the modifications with a 2026 date.
- **Corresponding source (§6):** the repository itself is the complete
  source; binaries on GitHub Releases and the playable web build on
  GitHub Pages are produced by CI (`.github/workflows/`) from this same
  public tree.
- **Copyright notices:** absent at every level of the chain. No file in
  this repository, in the upstream repository, or in the 1999 tree names
  a copyright holder; the LICENSE appendix placeholders are unfilled in
  both repositories.
- **Appropriate Legal Notices (§5d):** the port displays no in-game
  license/copyright notice; the 1999 original displayed none either
  (§5d imposes this only where the original program did).

## 5. Eclipse, Carapace, Media Pocket — the public record

**Eclipse** is a French PC demogroup, listed on Demozoo as group 7987
("Eclipse was a PC demogroup from France"), distinct from the US warez
group of the same name (Demozoo group 67881). The Demozoo page lists the
handles Rez, Profil, Hacker Croll and Light Show among its members, with
productions from 1993 onward. The upstream README publicly credits, by
name: "Rez (Christophe Résigné) - Musics; Profil (Frédéric Box) -
Graphics; Hacker Croll (Marc Radermacher) - Code (Brick Blaster +
WinEOS); Light Show (Régis Vidal) - Code (WinEOS)". The group's website,
`eclipse-game.com` ("Story of Demomakers", "Copyright © Eclipse Games
1992-2023"), is linked from the upstream README; its front page does not
mention Brick Blaster. A pouet.net search for "brick blaster" returned no
results. A MobyGames entry exists
(`mobygames.com/game/72268/brick-blaster/`) but returned HTTP 403 when
fetched, so its content is not verified here.

**Marc Radermacher** is publicly reachable via the website the upstream
README links, `https://www.edromel.com`. **david4599** is reachable via
GitHub.

**Media Pocket** is described by abandonware-france.org as a French
budget publisher in Boulogne-Billancourt active from 1999 ("L'objectif de
MEDIA Pocket est d'offrir au grand public des titres de qualité à un prix
accessible à tous"), which "ne fait plus parler d'elle" after five
collections; its game list on that page does not include Brick Blaster.
**Carapace** is described there as a French production company
(1992-2006, Paris) "dirigée par Frédéric Winkler et Philippe Cazer"; that
page does not mention Brick Blaster or Eclipse either. Both pages are
linked from the upstream README as the game's company and publisher. The
1999 French and Spanish commercial releases are hosted on archive.org
(`archive.org/details/brick-blaster-1999`,
`archive.org/details/brick-blaster-1999-spanish`).

One corroboration is worth recording because it is verifiable from the
shipped data rather than from any README: the 1999 `Blaster.scr` high-score
table, decoded with the game's own XOR codec (`HISCORE.ASM:454-466`, key
`(960 - i) & 0xFF`), holds `hacker croll` at rank 3 with 7,150 points. The
coder's demoscene handle is inside the released game data.

## 6. Discrepancies between this repository's README and the record

- The README states the sources were "open-sourced under GPL v3 in 2024
  by david4599" and the license is "inherited from the original
  BrickBlaster sources". The verifiable facts are narrower: the 2024
  repositories carry a GPL-3.0 LICENSE file, and the 1999 sources carry
  no license at all. Who selected GPL v3, and on what written authority,
  is undocumented.
- The README credits section states "In early 2024, Marc Radermacher
  entrusted the original source tree to david4599". The upstream wording
  is only "kindly providing the source code … and allowing to preserve
  it"; the date is consistent with the repositories' creation
  (2024-03-10) but "entrusted" is this README's paraphrase, not an
  upstream quote.

## 7. Open questions

Unanswered as of 2026-08-16. Phrased so they can be sent verbatim to
david4599 (and, where relevant, relayed to Marc Radermacher).

1. Does the GPL v3 publication of `david4599/BrickBlaster` cover the
   game data as well as the program source — specifically the graphics
   (`*.GIF`), palettes (`*.pal`), music (`*.mod`), sound samples
   (`*.IFF`), FLC videos, level files (`.lv0/.lv1/.lv2`), and the text
   of `Blaster*.cfg` — or only the code?
2. Is there a written statement from Marc Radermacher (email, letter)
   recording what he authorized in 2024, and can it be published in or
   referenced from the repositories?
3. Did Marc Radermacher hold the rights needed to license the whole work,
   including the music credited to Christophe Résigné ("Rez") and the
   graphics credited to Frédéric Box ("Profil"), or would those authors
   need to state their own position on the GPL v3 publication?
4. What was the rights situation with Carapace (Softplace) and Media
   Pocket for the 1999 commercial release — did any rights remain with
   those companies, or had they reverted to the authors?
5. Which copyright line should the LICENSE / source headers carry (both
   repositories currently name no copyright holder anywhere)?
6. For the port publisher (KonsomeJona): what is the ownership and
   intended licensing of the TakoHi name and logo files in
   `assets/takohi/`, given that they sit in a GPL v3 tree with no stated
   exception?

## 8. Sources

Local, examined 2026-08-16:

- `/mnt/e/dev/brickblaster/BrickBlaster/` — clean clone of
  `david4599/BrickBlaster` (LICENSE, README.md, `work400/` 1999 tree).
- This repository, branch `fix/asm-parity-2026-08` (LICENSE, README.md,
  CHANGELOG.md, `src/`, `assets/`, `data/`, `tests/`,
  `.github/workflows/`, `third_party/gif.h`, git history).

Online, consulted 2026-08-16:

- https://github.com/david4599/BrickBlaster — repository, README,
  LICENSE, issue tracker (empty).
- https://github.com/david4599/BrickBlaster-EOS-Archive — repository,
  README, LICENSE, issue tracker (empty).
- https://api.github.com/repos/david4599/BrickBlaster and
  https://api.github.com/repos/david4599/BrickBlaster-EOS-Archive —
  creation dates (both 2024-03-10), license detection (GPL-3.0).
- https://demozoo.org/groups/7987/ — Eclipse (French PC demogroup).
- https://demozoo.org/groups/67881/ — the unrelated US "Eclipse" warez
  group (checked to rule out confusion).
- https://www.eclipse-game.com — Eclipse group website.
- https://www.abandonware-france.org/compagnies/media-pocket-1019/ —
  Media Pocket.
- https://www.abandonware-france.org/compagnies/carapace-82/ — Carapace.
- https://www.pouet.net/search.php?type=prod&what=brick+blaster — no
  results.
- https://www.mobygames.com/game/72268/brick-blaster/ — entry exists;
  fetch returned HTTP 403, content not verified.
- https://archive.org/details/brick-blaster-1999 and
  https://archive.org/details/brick-blaster-1999-spanish — 1999
  commercial releases (as linked from the upstream README).
