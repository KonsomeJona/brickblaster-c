# BrickBlaster — Audit de parité ASM 1999 ↔ port C (13 août 2026)

**Code audité** : `61c1dd7` (sources figées depuis le 10 mai 2026).
**Référence** : `david4599/BrickBlaster` — `work400/Blaster/` (MAIN.ASM 7170 l., FILE.ASM,
DRAW.ASM, FLI32.ASM, HISCORE.ASM, MOUSE.ASM, EDITOR.ASM, FONTE.ASM, Blaster.inc,
Blaster*.cfg) + les fichiers de données de 1999 (`Blaster.scr`, `.lv0/.lv1/.lv2`, `.usr`).

**Méthode** : 6 agents en parallèle, un par cluster ASM, avec obligation de citer les deux
côtés et de vérifier les citations ASM présentes dans les commentaires du port. Chaque
finding P0 ci-dessous a été **revérifié à la main** après coup (relecture ASM + relecture C
+ mesure sur les données réelles).

Les audits d'avril (`audit-findings.md`, `audit-asm-faithful.md`, ~40 correctifs P0/P1)
ont été relus d'abord ; ce document ne re-liste pas ce qui est réellement corrigé.

---

## 1. La cause racine la plus rentable

`get_random` (MAIN.ASM:5103-5127) construit un masque de bits couvrant N, tire, puis
**rejette tant que le résultat dépasse N** :

```asm
@@cont:     call calc_random
            and eax,ecx        ; masque
            cmp eax,ebx        ; ebx = N
            ja  @@cont         ; recommence si > N
```

Il retourne donc **0..N inclus**, soit N+1 valeurs.

Le port utilise `GetRandomValue` de raylib — inclusive aux deux bornes, donc fidèle — dans
**14 des 16** sites. Les deux exceptions utilisent `rand() %`, exclusif, et ce sont
exactement les deux bugs :

| Site | ASM | Port | Conséquence |
|---|---|---|---|
| `random_options` MAIN.ASM:5474 | `get_random(23)` → 0..23 | `rand() % 23` → 0..22 | `COLLISION` (index 23) n'est **jamais** tiré |
| `random_options` MAIN.ASM:5511 | `get_random(freq-1)` → 0..freq-1, spawn si 0 → **1/freq** | `rand() % (freq-1)` → **1/(freq-1)** | freq=2 ⇒ `rand()%1==0` ⇒ **drop systématique** |

En difficulté facile, 15 des 23 entrées ont `freq=2` : chaque brique éligible lâche un
power-up, là où le jeu de 1999 en lâchait un sur deux.

`powerup.h:47` énonce pourtant la règle correctement (« probability = 1/(freq-1+1) =
1/freq ») ; `powerup.c:274` écrit l'inverse trois fichiers plus loin, et c'est cette
version-là qui est codée.

**Correctif** : remplacer les deux `rand() % N` par `GetRandomValue(0, N)`.
`powerup.c:236`, `powerup.c:275`, `game.c:160`, `game.c:168`.

---

## 2. P0 — divergences qui changent le jeu

### P0-1 · Le jeu a 40 niveaux, le port en joue 80
`search_level_number` (MAIN.ASM:5025) compte jusqu'au premier octet `0xFF`.
**Mesuré** : chaque fichier de monde fait 31 200 octets dont **exactement 15 600 octets
`0xFF`** — 40 niveaux réels, 40 emplacements vides. Le port fixe `LEVELS_PER_FILE 80`
(`level.h:24`) et justifie ce 80 par « validated by FILE.ASM:1205 » — cette ligne donne la
*capacité du fichier*, pas le nombre de niveaux jouables.

*Effet* : après le vrai niveau 40, 40 tableaux vides s'enchaînent (« ready ? », un tir,
jingle de fin de niveau) avant l'écran de victoire.
*Correctif* : calculer `level_number` au chargement en s'arrêtant au premier `0xFF`.
Corrige aussi P0-3 ci-dessous. `level.c`, `main.c:245`, `game.c:321`.

### P0-2 · Les briques dures demandent 7 coups au lieu de 4
`draw_brique` (MAIN.ASM:4968) plafonne au chargement : toute brique de résistance > 1
entre en jeu à exactement 4 (`and ebx,nombre_de_coups` / `or ebx,100b`). Le port lit le HP
littéral (`brick.c:48`).
**Recompté sur les 3 mondes**, en rejouant la logique ASM complète (y compris le remappage
transparente→normale qui précède le test) : **294 briques** (`0x27`, `0x67`, `0xA7`,
`0xE7`) valent 7 au lieu de 4. Les 331 briques `0xE4` et les ~1 800 transparentes tombent
juste.
*Correctif* : appliquer le plafond dans `brick_decode`.

### P0-3 · La rampe de vitesse arrive deux fois trop tard
Même cause que P0-1 : `game.c:321` divise par 80 au lieu de 40. Aggravé par
`change_speed_level` codé `3/4/4` (`game.h:108`) alors que `Blaster.cfg:45` livre `(2,3,3)`
— le parseur lit bien la clé, `main.c:428` ne l'injecte jamais.
*Effet* : toute la fin de campagne est plus molle qu'en 1999.

### P0-4 · Le mode démo ne démarre jamais
L'ASM passe directement en `PLAYING` avec une vélocité fixe (`sens_x=+3, sens_y=-4`,
MAIN.ASM:2761). Le port entre en `STATE_READY_TO_PLAY` (`screen_menu.c:149`) et attend un
tir qui ne viendra pas — `game.c:1854` exige `p1_fire || p2_fire`, et le retour anticipé de
`game.c:1899` empêche même le timer de démo de décrémenter. Un commentaire (`demo.h:46`)
renvoie à « la branche `@@demo` de main.c » qui n'a jamais existé.
*Effet* : l'attract mode affiche un terrain figé sur « ready ? ».

### P0-5 · Tout duel se termine par « draw »
`test_game_over` (MAIN.ASM:4674) : `dec player_nbs_ball / cmp -1 / jne @@cont` → le
**premier** joueur à court termine le match, sans aucun test de `dual_flag`. Le port fait
`game_over = p1_dead && p2_dead` (`game.c:1276`), le joueur mort continue de recevoir des
balles, et les deux étant morts à la fin, `winner` reste `-1`.

### P0-6 · Trois power-ups cassés
- **`GHOST`** : les balles fantômes explosent sur la première brique (`game.c:2205`) ; dans
  l'ASM le test ghost n'existe **que** dans la collision raquette (MAIN.ASM:4207) — elles
  cassent les briques et ne meurent qu'au contact du vaisseau. Le `continue` du port saute
  en plus la mise à jour de la brique touchée (état incohérent).
- **`ADD_MONSTER`** : passe par le compteur de spawn naturel (`monster.c:55`) au lieu
  d'invoquer directement comme MAIN.ASM:6764 — quasi no-op, et retarde le spawn suivant.
- **`MAGNETIC`** : n'expire jamais. Le `case` qui le désactive (`game.c:669`) est
  inatteignable car `apply_powerup` exclut explicitement `POWERUP_MAGNETIC` de
  `current_option` (`game.c:1134`).

### P0-7 · `FAST` / `SLOW` sont des rampes de 10 secondes
L'ASM remet `current_option` à `Off` en fin de `Refresh_Ball` (MAIN.ASM:2885) : l'effet ne
survit qu'à **une** passe, soit un seul cran de vitesse. Le port les classe « timed, 600
frames » (`powerup.c:318`) et applique un delta toutes les 3 (slow) ou 32 (fast) frames.
« Slow » plaque la balle à la vitesse minimale pendant 10 s.

### P0-8 · La palette par monde n'est pas portée — le monde par défaut a les mauvaises couleurs
`MAIN.ASM:97,483,491` patche le nom du fichier (`sprite0.pal` / `sprite1.pal`) selon le
monde, et `Read_Palette` (FILE.ASM:776) remplace les 768 octets de palette du sprite-sheet
à chaque chargement. Le port charge un unique `SPRITE.png` (`assets.c:57`) et ne
repalettise nulle part.

**Mesuré** : les deux palettes diffèrent sur **92 entrées** (indices 32-143). Après
conversion VGA 6→8 bits (×4), **191 des 191 couleurs** du `SPRITE.png` livré appartiennent
à `Sprite1.pal`, contre 104/191 pour `Sprite0.pal` — la feuille livrée est celle du monde 1.

*Effet* : dans le monde 0 (« blaster », le monde par défaut), briques, balles et raquettes
avaient en 1999 une teinte nettement différente. Le port affiche les couleurs « arcade »
dans les deux mondes.
*Correctif* : livrer deux feuilles de sprites, ou appliquer la palette au chargement du
monde.

---

## 3. P1 — écarts de correction

- **Power-ups ramassables pendant « ready ? »** — `detect_prise_option` sort dès sa
  première ligne hors `PLAYING` (MAIN.ASM:5605) ; la boucle du port (`game.c:2528`) n'est
  pas gatée. **C'est le candidat le plus direct au bug rapporté par un joueur** (« une
  balle est apparue sans prévenir ») : après une vie perdue, une multiballe résiduelle
  tombe sur le vaisseau immobile et lance des balles pendant que la principale est encore
  collée. Les `SPAWN_LOG` laissés dans le dernier commit devraient le montrer.
- **Fenêtre de spawn non ré-armée sur échec** (`game.c:1544`) — l'ASM consomme la fenêtre
  même quand le tirage rate (MAIN.ASM:5466). Amplifie la pluie de power-ups.
- **Un seul slot d'option par joueur en 1999** — l'ASM lit tous les effets portés à travers
  `player_current_option` : un joueur ne peut avoir qu'un effet à la fois, tout power-up
  instantané annule les effets des **deux** joueurs, et le timer de 600 frames est partagé.
  Le port a des timers indépendants par raquette : `SHOOT` + `LARGE` + `REVERSE` cumulables.
  Les correctifs d'avril ont réparé le routage P2 mais sont allés au-delà de l'ASM.
- **Ligne de mort manquante** — `detect_destruction` (MAIN.ASM:4526) condamne la balle dès
  `y ≥ 424`, 8 px sous le haut de la raquette ; le port autorise des sauvetages jusqu'à
  `y ≥ 471`. Le port est nettement plus permissif.
- **La balle de P2 n'est pas lancée en miroir** (MAIN.ASM:5309 `neg eax`) — les deux balles
  partent vers la droite (`game.c:1857`).
- **Vitesse raquette clavier P2 fixe à 6** — l'ASM la recalcule à chaque lancement à
  2×(vitesse de balle) (MAIN.ASM:5345), soit 4/6/8 selon la difficulté, +2 par palier.
- **Prise magnétique** : le port sort avant l'ajustement d'angle par zone (`collision.c:612`)
  que l'ASM applique quand même (MAIN.ASM:4214).
- **Retour au menu après game over** — l'ASM relance **automatiquement** une partie neuve
  (`NEW_PLAY` → `@@play_again`, MAIN.ASM:1194) après la table des scores, toujours affichée.
  Le port retombe au menu, et n'affiche la table que si le score qualifie.
- **Attract mode quasi inatteignable** — le port limite le timeout au menu 1 et remet le
  compteur à zéro dès que le curseur survole un quadrant (`screen_menu.c:260`) ; l'ASM
  décompte dans tous les menus et ne réinitialise que si la position **change**.
- **Noms de high score en majuscules** (`screen_hiscore.c:251`) — `HISCORE.ASM:316` force
  `or al,20h`, et la fonte de 1999 n'a pas de majuscules. Un `.scr` écrit par le port
  afficherait des noms vides dans le binaire DOS.
- **BOOM et DEATH absents à la perte de vie** — `destroy_vaisseau` (MAIN.ASM:4793) joue les
  deux à **chaque** mort. `SFX_EXPLOSION` est chargé (`audio.c:58`) et **jamais joué nulle
  part** (vérifié : zéro appel dans tout `src/`).
- **Règles suspendues en démo** — score, power-ups et vies sont neutralisés quand
  `demo_active` (`game.c:1520, 2452, 2543`) ; l'ASM n'a aucune garde `demo_flag` dans
  `inc_score` ni `detect_prise_option`. L'attract de 1999 est une vraie partie.

### Rendu

- **Balle de P2 verte, pas bleue** — `MAIN.ASM:2803` : `ball_orange_o` puis `+9` pour P2 en
  duel, soit +9 **pixels** dans l'atlas = la balle verte (dorée sous iron, ghost vert). Le
  commentaire `draw.c:570` justifie l'inversion en affirmant que « ball_orange_o est à
  l'offset 9 » — il vaut `640*9 = 5760`. Toute la logique de couleur du duel repose sur
  cette lecture. Le port choisit en plus la couleur par **index de balle** (`i >= 1`) et non
  par propriétaire.
- **Icônes de power-ups tombantes corrompues** — `POWERUP_SPRITE_Y_OFF` (`draw.c:139`)
  envoie BALL_3/6/9/20 à Y=804, soit 2 px dans la ligne « fade » (802), plus des flips
  verticaux et un miroir. L'ASM blitte toujours la ligne 752, intacte ; la ligne fade ne
  sert qu'à l'icône du HUD. Correctif : `Y_OFF` et les tables de flip à zéro.
- **Icône du power-up actif absente du HUD** — `MAIN.ASM:5691` pose l'icône en bas à gauche
  pendant toute la durée de l'effet ; le port n'affiche qu'un texte.
- **Animation « break » : stride 11 au lieu de 10** (`draw.c:952`) — l'ASM avance de
  `size_x + next_shape` = 9+1 ; les frames sont donc lues de travers, décalage cumulé
  jusqu'à 4 px. Pire, l'animation est déclenchée sur **chaque brique détruite et chaque
  balle perdue**, avec une citation ASM inventée : les deux seuls sites d'origine sont
  l'éclatement d'une balle fantôme sur la raquette.
- **Compteur de vies dans le mauvais sens** — l'ASM empile les billes vers la **gauche** par
  pas de 12 depuis x=518 ; le port va vers la droite par pas de 11 et déborde sur le panneau
  latéral dès la 2ᵉ vie. La rangée P2 est de plus affichée en coop, alors que l'ASM la
  réserve au duel.
- **Score et niveau non formatés** — l'ASM affiche `000150` et `01` sur la largeur exacte du
  panneau ; le port fait `%d` centré.
- **FLC des crédits à 18 fps au lieu de 12** (`screen_credits.h:16`) — `FILE.ASM:96` attend
  5 vsyncs par frame. À noter : `audit-asm-faithful.md` listait ce point dans ses
  *Confirmed matches*.
- **Piliers de bordure absents et noircissement de palette non porté** — `create_border`
  (MAIN.ASM:5825) stampe 5 tuiles 42×96 à x=70 et x=528, et `load_file_fond` force les
  entrées 0 et 15 de la palette du fond à noir. Le port remplace tout par un voile sombre
  semi-transparent « for a polished look on mobile » — qui compense sans le savoir la patte
  palette non portée.
- **Scanner LED « KITT » sur la raquette** (`draw.c:695`) — ajout non-ASM assumé en
  commentaire, de la même classe que le screen-shake purgé en avril par la règle HARD.
- **Explosion de monstre 2× trop brève** — l'ASM avance d'une frame toutes les 2 vsyncs
  (~30 frames) ; le port décrémente par frame (~15). L'offset −19 contredit sa propre
  citation (« sub pos,16 »).
- **Score P2 fantôme en coop** (`draw.c:846`) — un « 0 » orphelin à x≈4, hors panneau ;
  `FONTE.ASM:10` ne l'imprime qu'en duel.

---

## 4. P2/P3 — présentation et hygiène

Bannière de power-up affichée 600 frames au lieu de 100 (`DELAI_INFO` défini dans
`constants.h:148`, jamais consommé) · `.usr` réduit de volumes 0..64 à des booléens, VU-mètre
du panneau absent · écrans de victoire `final_text`/`final_dual` en anglais codé en dur
alors que les trois `.cfg` portent les versions FR/ES · `final_dual` affiché à la victoire
au lieu du game over duel · son inventé sur power-up perdu au sol (`iff_lost_option` vaut
`0` dans l'ASM = silence) · `iff_multi` mal mappé · pas de fondu sur les transitions ·
rebond mur recalé en position (l'ASM ne touche jamais la position) · explosion monstre
décalée de 19 px au lieu de 16 · `last_random` (jamais deux fois le même tirage) non porté ·
téléportation 1 px trop courte.

**Bannière de power-up : l'audit avait raison, ma correction du §6 ter avait tort.**
`DELAI_INFO` EST utilisé : `dec current_option_count / cmp current_option_count,DELAI_INFO /
je @@display_info_off` (MAIN.ASM:6304-6306), avec `DELAI_INFO = DELAI_OPTION-100 = 500` et
le compteur armé à 600 — le bandeau dure donc **100 frames**, pour tous les power-ups. Mon
« non référencé dans l'ASM » du §6 ter venait d'un `grep` vide, et `grep` est cassé sur ces
fichiers (voir l'avertissement en tête de §6 quater). Corrigé dans le port.
`DELAI_INFO_SOUND`, lui, est bien mort : sa seule occurrence (MAIN.ASM:6307) est commentée.

Constantes mortes côté port : `CREDITS_SLIDE_TIMEOUT`, `DELAY_INTRO_1`, `SB_FREQ`.
Chaînes i18n mortes : `STR_READY`, `STR_GAME_PAUSED`, `STR_GAME_OVER`, `STR_DEMO_LABEL`.

---

## 5. Le pattern à corriger dans la méthode, pas seulement dans le code

Trois findings sont des correctifs d'avril qui ont **entériné une hypothèse jamais
vérifiée** :

- Le fix P0-2 (duel) portait la mention « ASM disables losing paddle but keeps game until
  BOTH exhausted — **verify** ». La vérification n'a pas été faite ; l'inverse a été codé.
- `final_dual` a été corrigé **au mauvais endroit** (victoire au lieu de game over).
- `IRON BALL` était marqué « à trancher » puis a été minuté à 600 frames alors que l'ASM le
  rend permanent par balle.

Plusieurs commentaires du port citent l'ASM avec assurance à des endroits où la ligne citée
dit autre chose : `main.c:238` invoque un label `new_play` qui n'existe pas ;
`powerup.c:236` affirme « range is 0..22 » ; `monster.h:13` se trompe de deux lignes ;
`audio.c:44` se déclare en « déviation de portage » sur un comportement en réalité fidèle.

**Règle proposée** : aucun correctif de fidélité n'est mergé si le commentaire ne cite pas
une ligne ASM qui a été ouverte et relue. Une citation invérifiée vaut moins que pas de
citation — elle fait passer une supposition pour une mesure.

---

## 6. Ordre de correction suggéré

1. **`GetRandomValue` aux 2 sites** — 4 lignes, corrige le drop garanti + `COLLISION`.
2. **`level_number` calculé sur la sentinelle `0xFF`** — corrige la fin de campagne et la
   rampe de vitesse d'un coup.
3. **Plafond à 4 coups dans `brick_decode`** — 3 lignes.
4. **Gate `STATE_PLAYING` sur la collecte de power-ups** — corrige probablement le bug
   rapporté par le joueur ; permettrait de retirer les `SPAWN_LOG` du dernier commit.
5. **Démo : entrer en `PLAYING` avec la vélocité fixe de MAIN.ASM:2761.**
6. **Duel : game over au premier joueur épuisé.**
7. **Icônes de power-ups** : `POWERUP_SPRITE_Y_OFF` et les tables de flip à zéro — une ligne
   chacune, corrige des icônes visiblement abîmées.
8. **Animation « break »** : stride 10, et la retirer des deux déclencheurs inventés
   (destruction de brique, perte de balle).
9. `GHOST`, `ADD_MONSTER`, `MAGNETIC`, `FAST`/`SLOW` — un par un, chacun avec sa citation.
10. **Palette par monde** — plus lourd (deux feuilles de sprites à produire ou une
    repalettisation au chargement), à planifier à part.
11. Le reste par gravité décroissante.

Les points 1 à 3, 7 et 8 sont de très petits diffs pour un effet de jeu important. Le 4 est
le seul qui réponde à un rapport de joueur réel.

---

## 6 bis. Ce qui a été corrigé (branche `fix/asm-parity-2026-08`)

Deux vagues de correctifs, séparées par une passe de vérification **adversariale** dont la
consigne était de chercher ce qui restait faux, pas de confirmer.

**Vague 1** — les correctifs listés plus haut. La vérification a montré qu'ils avaient
introduit **une régression** et laissé **cinq corrections à moitié** :
- `draw.c` mesurait l'usure d'une brique comme `raw − hp` ; le plafond ayant changé `hp`
  sans changer `raw`, les 294 briques concernées s'affichaient aux trois quarts détruites
  **dès l'apparition du niveau**. Corrigé par un champ `hp_initial`.
- La démo démarrait mais **gelait au premier passage de niveau** (`NEW_PLAY` forçait
  `READY_TO_PLAY` ; l'ASM saute par-dessus, MAIN.ASM:961-967).
- `IRON`, `GHOST` et `TELEPOD` étaient déclarés one-shot mais la queue du pipeline posait
  `current_option` + 600 frames et les annulait quand même.
- La rampe de vitesse était devenue **trop rapide** : le diviseur était corrigé, mais
  `change_speed_level` restait 3/4/4 au lieu du `(2,3,3)` du cfg — 60 cellules sur 120.
- La fenêtre de spawn n'était toujours pas consommée sur échec (×2,48 en difficile).

**Vague 2** — les points ci-dessus, plus : balle P2 en miroir, éclatement filtré par
propriétaire en duel, quatre familles de sprite d'éclatement, cadence de l'animation,
compteur de vies jusqu'à 19, double KO en duel (l'ASM ne connaît pas le match nul —
HISCORE.ASM:133-140 ne teste que le compteur de P2).

**Générateur aléatoire — décision : portage fidèle.** `calc_random`/`get_random` sont
portés bit à bit dans `asm_random.c`, avec leurs défauts : `alea1` n'est jamais écrit, les
lectures CMOS sont commentées, la période est de 4811 avec premier cycle à 2603.
Équivalence prouvée sur 100 000 tirages contre un émulateur x86 indépendant. Coût mesuré et
assumé : l'index 9 sort +23,4 % au-dessus de l'uniforme, les raretés à −7 %.
`wait_synchro` (DRAW.ASM:110) faisant avancer le générateur **à chaque frame**, le tick est
porté lui aussi — mais la parité trace-pour-trace reste hors d'atteinte, la cadence VGA de
1999 n'étant pas celle du port. Ce qui est acquis, c'est le grain statistique.

`create_ball` et `Random_Speed` sont portés également : les multiballes partent désormais
en positions et directions tirées au sort, non plus en éventail déterministe. **C'est le
changement le plus perceptible manette en main.**

Non porté volontairement : le fond aléatoire de l'écran des scores (invention du port, sans
site ASM — le brancher polluerait l'état global partagé).

## 6 ter. Queue P1 traitée le 16 août 2026 — et vérifiée en exécution

Cette passe a fermé le trou déclaré au §7 (« aucun finding n'a été confirmé en exécutant
le jeu »). Le port a été compilé et **lancé** sous WSLg, et les correctifs visuels ont été
confirmés sur des captures de la fenêtre réelle.

**Corrigé, chaque ligne ASM ayant été rouverte :**

- **`SFX_EXPLOSION` + `SFX_GAME_OVER` à chaque perte de vie.** `destroy_vaisseau`
  (MAIN.ASM:4792) ouvre sur `play_sound iff_explosion` / `play_sound iff_game_over`, et
  `test_game_over` l'appelle depuis **les deux** branches — MAIN.ASM:4685 (game over) et
  MAIN.ASM:4703 (`@@cont`). `SFX_EXPLOSION` n'était joué nulle part.
- **FLC des crédits 18 → 12 fps.** FILE.ASM:96-99 `mov ecx,5` / `loop @@wait`. Identique à
  FILE.ASM:164-170 dont le port tirait déjà `FINAL_FPS 12.0f` : la valeur 18 contredisait
  la constante sœur du port lui-même, et son commentaire (« standard FLC playback rate »)
  était une invention — la cadence est celle que compte la boucle d'attente.
- **Vitesse clavier P2.** MAIN.ASM:5345-5348 recalcule `speed_counter` à
  `2 × |ball_2.sens_x|` à chaque lancement (4/6/8 selon la difficulté, +2 par palier).
  `MOUSE.ASM:77 speed_counter dd 6` n'est que l'initialiseur du loader.
- **Ligne de mort.** `detect_destruction` (MAIN.ASM:4526-4541) tue la balle dès que son
  **centre** atteint `limite_y + cursor_size_y/2` = 416 + 12 = 428, soit `pos_y ≥ 424`, et
  seulement en descente (`js @@end`). Le port attendait la sortie d'écran, `pos_y ≥ 471` —
  47 px de rattrapage offerts.
- **Gardes `demo_active` retirées (5 sites).** Vérification décisive : `demo_flag`
  n'apparaît **dans aucun** autre module — zéro occurrence dans FONTE.ASM (`inc_score`),
  MOUSE.ASM et HISCORE.ASM — et jamais dans `detect_prise_option` ni `test_game_over`.
  L'attract mode de 1999 marque, ramasse et applique les power-ups.
- **Piliers de bordure.** `create_border` (MAIN.ASM:5825-5852), 5 tuiles 42×96 par côté à
  x=70 et x=528. Le voile sombre du port est conservé — il compense la patte palette de
  `load_file_fond`, non portable — mais les piliers sont désormais dessinés par-dessus.
- **Icône du power-up actif dans le HUD.** MAIN.ASM:5688-5694, à `panel_option` (122, 446),
  échantillonnée sur `option_fade_o` (rangée 802). C'est l'unique consommateur de cette
  rangée, ce qui confirme a posteriori que les icônes tombantes ne doivent jamais y toucher.

**Hors parité, trouvé en vérifiant la table des contrôles du README :** `A` et `D` étaient
liées **à la fois** au J1 et au J2 en clavier 2 joueurs (`input_frame.c`) — une seule
touche déplaçait les deux raquettes. Les alias du J1 sont désactivés quand le J2 est au
clavier. Le J1 de 1999 n'avait pas de liaison clavier du tout (`Refresh_Mouse`).

**Deux erreurs de cet audit lui-même, corrigées :**

- ~~Le §4 se trompait sur la bannière de power-up~~ — **c'est cette correction-là qui était
  fausse**, et elle mérite d'être gardée comme avertissement. J'avais conclu « `DELAI_INFO`
  n'est référencé nulle part dans l'ASM » à partir d'un `grep` vide. Or `grep` renvoie
  silencieusement zéro résultat sur ces fichiers dans cet environnement : `DELAI_INFO` est
  bien lu en MAIN.ASM:6305 et fixe la durée du bandeau à 100 frames. Le §4 avait raison.
  Depuis, toute recherche dans l'ASM passe par `awk` et une lecture directe.
- Le §3 donnait la ligne de mort à « `y ≥ 424`, 8 px sous le haut de la raquette ». Le
  seuil est bien 424, mais l'écart est de 12 px (`cursor_size_y/2` = 25/2), et la
  comparaison porte sur le centre de la balle, pas sur son bord.

**Nouvelle question ouverte — le voile latéral.** En posant les piliers on a vérifié
`load_file_fond` (FILE.ASM:374-386) : il met à zéro les entrées **0 et 15 de la palette du
GIF de fond entier**, puis appelle `create_border` (FILE.ASM:388), puis copie dans
`background_buffer`. Ce n'est donc **pas** un assombrissement des colonnes latérales, et le
voile semi-transparent du port n'en est pas l'équivalent — sa vraie justification, écrite
dans le port, est de masquer les panneaux vifs de l'art source. Les piliers ne couvrant que
70..112 et 528..570, le voile reste visible de part et d'autre : de l'art authentique posé
sur un assombrissement inventé. Le voile devrait probablement disparaître, le blackout de
palette étant refait au chargement dans `assets.c` (précédent existant au même endroit pour
le logo). Changement visuel à valider seul, non fait ici.

**Toujours ouvert** (inchangé) : slot d'option unique par joueur, fondus `Shade_On/Off`,
relance automatique après game over, table des scores toujours affichée, noms de high score
en minuscules, `final_text`/`final_dual` en anglais codé en dur, écran de réglages sonores
non câblé.

## 6 quater. Passe de parité totale — 16 août 2026

Objectif : fermer **toutes** les divergences restantes. Quatre clusters instruits en
parallèle (règles, audio, rendu, textes/sauvegardes), chaque verdict rendu en citant les
deux côtés. Tout ce qui suit est appliqué, compilé et vérifié en exécution.

### Les deux découvertes structurelles

**La cadence était fausse d'un facteur 3.** Le `MAKEFILE` assemble `tasm32 … /dWIN32` et
lie `system wineos` : le binaire de 1999 est la variante **WIN32/WinEOS**, donc toute la
branche DOS de `wait_synchro` — poll VGA `0x3DA` *et* pacer PIT `timer_counter >= 8` — est
compilée hors du binaire (DRAW.ASM:111-152, `ifndef WIN32 … else … endif`). Le vrai
cadenceur est `Wait_Vbl` de WinEOS : **une frame par vertical blank en 640×480, soit
60 Hz**, et `wait_synchro` n'est appelé qu'une fois par tour de boucle (MAIN.ASM:1097).
Le port compensait un « ASM à 18 fps » par un ×3 sur 7 sites — 18,2 fps est ce que donnait
le chemin DOS (8 ticks d'un PIT à 145,6 Hz), du code jamais livré. Conséquence mesurable :
`SPEED_DELAI = 1500` vaut 25 s au niveau 1, le port en faisait 75. Le mapping est **1:1**,
ce que confirment `DELAI_OPTION = 600` et `DELAI_DATTENTE = 600` (10 s chacun), que le port
utilisait déjà tels quels. Preuve consignée en tête de `game.h`. Le « 70 Hz VGA » cité au
§7 est également faux : 640×480 se rafraîchit à 60 Hz, le 70 Hz est le mode 13h.

**Les fonds du monde 1 étaient de mauvaises conversions.** `load_file_fond` appelle
`Create_Palette` avec **`ecx = 16*3`** (FILE.ASM:384-386) : seules les entrées 0..15 de la
palette du GIF de fond comptent, les index ≥ 16 sont rendus avec `spriteN.pal`, chargée une
fois par monde (FILE.ASM:250-255). Les 8 PNG `01_*` avaient été convertis via la palette du
GIF, où les entrées 229..243 sont un aplat rouge unique — d'où les « panneaux rouges vifs de
l'art source » que le voile latéral du port avait été inventé pour masquer. **858 718 pixels
corrigés** en régénérant via `Sprite1.pal` ; 0 pixel d'écart dans le terrain, tous les écarts
étaient dans les panneaux. Le voile est supprimé et la machinerie bleu-gris d'origine
apparaît. Le monde 0 était déjà correct (5 px sur `00_07`). Vérifié aussi : `Blaster.png`
(menu) est légitimement une conversion naïve, `Load_Picture_Menu` utilisant
`Create_Palette` avec `ecx = 256*3` (FILE.ASM:216-220).

### Règles de jeu

- **Un seul slot d'option par joueur.** `player_option[2]` + `sync_paddle_from_option`,
  dérivé chaque frame comme `detect_large/small_cursor_*` et `detect_shoot_*`
  (MAIN.ASM:1071-1076). Preuve qu'aucun compteur par raquette n'existe :
  `option_small_ship_p`, `option_large_ship_p` et `option_reverse_p` sont des `ret` nus
  (MAIN.ASM:6703, 6710, 6717) ; les `count_tir_*` sont des compteurs d'animation de canon
  (MAIN.ASM:1897-1903). Tout power-up instantané efface les **deux** joueurs, le timer de
  600 frames est partagé, `MAGNETIC` restaure `old_option` dans les trois slots
  (MAIN.ASM:6745-6750), et le gros tir se consomme (MAIN.ASM:1947-1948, 2156-2157).
- **Attract mode** dans **tous** les menus (`get_menu` n'a qu'une boucle,
  MAIN.ASM:279-323), réarmé uniquement quand la position du curseur **change**
  (`detect_reset_ecx`, MAIN.ASM:569-583) — un survol immobile bloquait la démo
  indéfiniment. Reset de `control_2 = COMPUTER` au timeout (MAIN.ASM:314).
- **Prise magnétique** : `detect_magnetic_player_1` est un simple setter qui `ret`
  (MAIN.ASM:4398-4439), il n'interrompt pas la séquence — la balle accrochée reçoit donc
  aussi `neg sens_y` et l'ajustement d'angle par zone (MAIN.ASM:4219-4243).
- **Fin de partie** : la table des scores est **toujours** affichée
  (`_Display_score` est inconditionnel, HISCORE.ASM:178-212 ; seule la saisie du nom est
  conditionnelle, HISCORE.ASM:235-237 / 281-282), et une partie neuve est relancée
  automatiquement (`NEW_PLAY` → `@@play_again` → `start_new_game`, MAIN.ASM:1092-1093,
  1194-1211, 995-1028) au lieu d'un retour menu.
- **Score coop** : vérifié **conforme**. `inc_score` force `ebp = player_1` hors duel
  (FONTE.ASM:88-91), et le port ne sépare les compteurs qu'en `game_mode == 2`.

### Audio

Table reconstruite depuis les **alias de labels** de FILE.ASM:728-770, recoupée par
17 `loadsample` pour exactement 17 `.IFF` sur disque :

- `iff_death` ≡ `iff_game_over` → `death.iff` ;
- `iff_incassable` ≡ `iff_cursor` ≡ `iff_multi` → `wall.iff` ;
- `iff_lost_option` n'a **aucune** entrée `name_iff_*` → jamais chargé → **silence**.

Corrigé : `iff_multi` est une brique **multi-coups survivante** (`cmp B [esi+ebx],absente /
jne @@redraw_brique`, MAIN.ASM:4004-4005), pas une multiballe — `SFX_MULTI_BALL` supprimé,
le son de brique a désormais ses trois cas. Sons inventés retirés : spawn de balles,
`TELEPOD` sur le power-up, `SPEEDUP` sur `FAST`/`SLOW` (les quatre handlers sont des `ret`,
MAIN.ASM:6613-6636), et power-up touchant le sol. `SFX_POWERUP_LOST` renommé
`SFX_DEL_MONSTER` : il jouait déjà le bon fichier sous un nom trompeur. Le commentaire de
`audio.c` qui se déclarait « port deviation » sur le rebond raquette était faux — le
comportement était fidèle.

### Rendu

- **Rebond mural** : `detect_colision_wall` (MAIN.ASM:3497-3535) est purement prédictif et
  n'écrit **jamais** la position ; le port recalait la balle sur le mur, décalant sa
  trajectoire de jusqu'à |v|−1 px à chaque rebond.
- **Ordre z** : les monstres sont déclarés après les balles et les vaisseaux
  (MAIN.ASM:7096 vs 7066/7087) et `Draw_sprites` parcourt la table en ordre mémoire
  (DRAW.ASM:426-434) — l'explosion 70×70 passait sous la raquette.
- **Explosion de monstre** : arrêt à `to_delete == 1` (14 avances, DRAW.ASM:396-400) et
  première avance = **reset** et non incrément (`cmp current_shape,1 / jbe @@reset`,
  DRAW.ASM:403-416, `current_shape` initialisé à 1 par MAIN.ASM:3080).
- **Téléportation** : les quatre sondes utilisent la taille **pleine** du sprite
  (MAIN.ASM:1543-1559), pas taille−1.
- **Icônes de power-up** : `get_powerup_rect` expose les trois rangées de l'atlas comme
  décalages nommés (`OPTION_ROW_FALL/P2/FADE`) au lieu d'une mutation après coup.

### Textes, scores, sauvegardes

- **`final_text` / `final_dual`** en FR/EN/ES depuis les `.cfg` (`read_text_final`,
  FILE.ASM:1089-1110), patch `winner` par **`memcpy` de 4 octets à l'index 16**
  (HISCORE.ASM:133-138) — bug d'alignement du `.cfg` espagnol reproduit tel quel.
  Géométrie exacte : x = 120, y = 52, interligne 30 (FONTE.ASM:171-184, 370, 416), tracés
  par-dessus la dernière image du FLC (`transparence = On`) et non sur du noir.
- **`final_dual` déplacé de la victoire vers le game over duel** : `Display_score_from_final`
  sort sur `dual_flag` (HISCORE.ASM:28-29) alors que `Display_score` n'affiche `final_dual`
  **que** en duel (HISCORE.ASM:109-141). Le port avait les deux inversés. La bannière
  « player 1 wins / draw » de l'overlay est supprimée : elle n'existe pas, et le match nul
  est impossible (HISCORE.ASM:134 ne teste que P2).
- **Volumes `.usr`** : deux octets 0..64 (FILE.ASM:815-816), portés tels quels de bout en
  bout. Le port les réduisait à des booléens et **écrasait le fichier du joueur à la
  sortie**. Le **VU-mètre musique du menu** est implémenté (Blaster.inc:44-52, dessin
  MAIN.ASM:788-820, glissé `detect_button_music` MAIN.ASM:649-689) ; le panneau `M`/`S` de
  la pause, qui n'existe pas dans l'original, est retiré. Le bandeau noir plein écran que
  le port peignait sous le titre du menu masquait le VU-mètre : supprimé, le titre est
  imprimé à `panel_menu` = (155, 446) par-dessus l'art comme dans l'original.
- **Saisie des scores** : la roue expose les **45 glyphes** de la fonte
  (FONTE.ASM:418-419) au lieu de 37 — la ponctuation était inaccessible alors que le
  `.scr` de 1999 contient `pas cool !!!!!`.
- **Éditeur** : les deux lignes d'aide inventées sont remplacées par la seule bannière
  `option_text_editor` du `.cfg` (EDITOR.ASM:54-55).
- **Vérifiés conformes** : codec XOR et format `.scr` (le `.scr` de 1999 se décode
  correctement avec l'algorithme du port — un fichier écrit par le port est relisible par
  le binaire DOS), noms en minuscules, et les 24 libellés de power-up FR/EN/ES, comparés
  octet à octet aux `.cfg` (87 chaînes, 0 écart). Une seule correction : `cooperado.` en ES.

### Erreurs de cet audit corrigées

- `DELAI_INFO` / `DELAI_INFO_SOUND` sont morts **dans l'ASM aussi** — le port était fidèle.
- La ligne de mort est bien à 424 mais l'écart est de 12 px (`cursor_size_y/2`) et la
  comparaison porte sur le **centre** de la balle.
- Le « 70 Hz VGA » du §7 : la résolution est 640×480, donc 60 Hz.
- L'affirmation d'un son inventé sur le rebond raquette : le port était fidèle.

### Ce qui restera hors d'atteinte

La conversion audio `.iff`/`.mod` → WAV n'est pas réversible au bit près, et le décodeur
GIF/LZW du port n'est pas celui de 1999. Ces deux points ne peuvent pas être « corrigés »,
seulement documentés.


## 6 quinquies. Double passe croisée ASM ↔ C — 16 août 2026

Deux agents lancés en parallèle, un par sens, avec interdiction d'affirmer sans avoir
ouvert les deux côtés. Chaque finding retenu ci-dessous a ensuite été **rouvert à la main**
avant correction.

### Sens ASM → C (omissions) — 16 findings sur 17 appliqués

Le plus lourd : **la perte de vie ne réinitialisait presque rien**. `test_game_over` sort en
`stc` sur la branche « on continue » et la boucle fait `jc start_game` (MAIN.ASM:1089) ;
`start_game` (1036-1045) appelle `init_sprites` — `sprite_status,Off` sur **tous** les
sprites hors panneau — plus `reset_magnetic` et `reset_ghost`, puis tombe dans
`rebuild_all` → `init_monster`. Le port ne vidait que les power-ups. Même chemin au
changement de niveau (`_next_level` finit par `jmp start_game`, MAIN.ASM:968).

Ensuite : **un seul clic lance les deux balles** (`detect_start_game` n'a qu'un
`call read_click`, MAIN.ASM:5286, et `read_click` agrège souris/CTRL/joystick) ; **le gros
tir traverse tout**, incassables et monstres compris (`sprite_rebond,Off` MAIN.ASM:1946 →
jamais de `new_direction` 3863-3864 → `change_direction` sort avant `@@shoot_off`) ; **en
duel l'option est réservée à celui qui a cassé la brique** (5626-5630), avec l'easter egg
qui la **donne à l'adversaire** si le ramasseur tient son bouton (5655-5670) ; **5 slots de
monstres** et non 4 (`nbs_monster = 4` n'apparaît que dans une ligne commentée,
MAIN.ASM:2932) ; monstres rebondissant sur les transparentes (3993-3994 + `stc`) ; balle de
fer traversant les téléporteuses ; GHOST refait (`set_ghost` marque **toutes** les balles du
ramasseur, `unghost_one_ball` en dé-marque une) ; positions de tir (18 px au-dessus de la
raquette pour les deux types, gros tir centré sur la largeur **courante**) ; retirage sur
fréquence 0 (`@@again` est une boucle, 5508-5509) ; graine `speed_delai` ; tir automatique
en démo (`@@auto_shoot`, 1907-1920) ; scalaires du `.cfg` injectés ; timer d'option gelé
hors `PLAYING` (6298-6300) ; **bandeau de power-up à 100 frames** (voir ci-dessous).

Non appliqué, assumé : la barre d'espace qui relance une partie neuve (MAIN.ASM:1173-1181)
— Espace est le tir dans le port.

### Sens C → ASM (hallucinations) — 2 025 citations extraites, ~620 vérifiées

Taux constaté : **~88 % exactes, 7-8 % à ±1-4 lignes, 4-5 % franchement fausses.**

**Un bug de gameplay mort, introduit par le refactor du slot d'option** : `POWERUP_NIGHT`
s'auto-annulait. Son `case` posait `night_active = 1`, puis la branche `default:` du second
switch le remettait à 0 dans le même appel. La pastille jouait son son et ne faisait plus
rien. Le `night_active = 0` ne concerne en réalité que iron / telepod / fast / slow, dont
`@@reset_current_option` (MAIN.ASM:2895-2898) laisse `current_option` visible une frame à
`detect_init_palette` (6667-6677).

Autres corrections de fond : la **vie bonus annule l'option des deux joueurs**
(`option_new_life_p`, MAIN.ASM:6441-6443) ; la **balle de fer déclenche son et reflet** sur
l'incassable (`@@collision` joue `iff_incassable` et arme le reflet quel que soit le rebond,
4048-4062) ; l'ordre par balle est **raquette → mur** et non l'inverse (2864-2871).

**Citations fausses corrigées** : ~20 numéros `Blaster.inc` systématiquement décalés dans
`draw.c` (valeurs justes, lignes fausses — `brique_classic_o` 342→345, `brique_beton_o`
350→342, `vaisseau_large_1_o` 281→284…) ; le label **`load_decor` qui n'existe nulle part**
(le vrai chemin est `next_fond` → `load_file_fond`) ; `MAIN.ASM:347` recyclé dans 7 endroits
pour le ramassage en jeu alors que c'est le **clic de menu** (le vrai site est 5708) ;
`MAIN.ASM:133-138` au lieu de `HISCORE.ASM` pour le patch `winner` ; une douzaine
d'off-by-1-à-4.

**Deux justifications entièrement inventées, supprimées** : le port affirmait que l'ASM
n'atteint jamais `detect_game_over` en démo — avec **deux mécanismes différents et
contradictoires** selon le fichier (« demo_timer exit » / « read_click exit »). Ni l'un ni
l'autre n'existe : MAIN.ASM:1088 est inconditionnel et `test_game_over` n'a aucune garde
`demo_flag`. L'attract mode de 1999 perd bien ses vies. Le respawn du port est conservé mais
désormais **étiqueté comme déviation**, pas comme fidélité. De même, l'explication du
`sar eax,1` de la table de trajectoire (« demi-pas avant le bord de la balle ») était fausse :
le résultat est `pixel<<15`, une coordonnée hors champ que `detect_brique` rejette toujours —
le sous-pas 0 ne teste jamais rien. Le code était fidèle, la justification inventée.

### Ce que la double passe a validé

`asm_random.c` est ressorti **irréprochable** : transcription bit-exacte, et ses statistiques
« mesurées » (période 4811, queue 2603, index 9 à +23,4 %) reproduites par simulation
indépendante. Idem pour les 24 entrées de la table d'options, les 120 citations
`Blaster.inc` de `constants.h` (100 % justes), la table de rebond à 16 cas, tout le pipeline
`detect_prise_option`, `inc_score`/`dec_score`, le format `.scr` et son codec XOR, et les
29 sites `play_sound` — tous exacts.

### Leçon de méthode

`grep` renvoie **silencieusement zéro résultat** sur ces fichiers ASM dans cet
environnement. C'est ce qui m'avait fait écrire au §6 ter que `DELAI_INFO` n'était référencé
nulle part : il l'est (MAIN.ASM:6305) et fixe la durée du bandeau à 100 frames. Toute
recherche passe désormais par `awk` et une lecture directe.

## 7. Ce que cet audit n'a pas couvert

Fondus `Shade_On/Off` (palette 32 pas) vs fondus alpha du port · ordre z exact de
`Begin_Sprites` · durées de fondu de `display_intro` (définies dans la lib EOS, hors dépôt) ·
visuels de `EDITOR.ASM` et `HISCORE.ASM` · décodeur GIF/LZW · fidélité de conversion audio
`.iff`/`.mod` → WAV · effets tracker (`EFFECT.ASM`, `MIXING.ASM`) · code Android/Wear/mobile ·
chemins de sauvegarde Android · comportement à 70 Hz VGA vs 60 fps (l'équivalence
frame-à-frame retenue par le port a été supposée, pas démontrée) · **aucun finding n'a été
confirmé en exécutant le jeu** : tous les verdicts viennent de lecture croisée et de mesure
sur les fichiers de données. Le P0-4 (démo) mériterait une confirmation runtime de 30 s.
