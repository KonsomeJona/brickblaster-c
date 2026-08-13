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

Constantes mortes : `DELAI_INFO`, `DELAI_INFO_SOUND`, `CREDITS_SLIDE_TIMEOUT`,
`DELAY_INTRO_1`, `SB_FREQ`. Chaînes i18n mortes : `STR_READY`, `STR_GAME_PAUSED`,
`STR_GAME_OVER`, `STR_DEMO_LABEL`.

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

## 7. Ce que cet audit n'a pas couvert

Fondus `Shade_On/Off` (palette 32 pas) vs fondus alpha du port · ordre z exact de
`Begin_Sprites` · durées de fondu de `display_intro` (définies dans la lib EOS, hors dépôt) ·
visuels de `EDITOR.ASM` et `HISCORE.ASM` · décodeur GIF/LZW · fidélité de conversion audio
`.iff`/`.mod` → WAV · effets tracker (`EFFECT.ASM`, `MIXING.ASM`) · code Android/Wear/mobile ·
chemins de sauvegarde Android · comportement à 70 Hz VGA vs 60 fps (l'équivalence
frame-à-frame retenue par le port a été supposée, pas démontrée) · **aucun finding n'a été
confirmé en exécutant le jeu** : tous les verdicts viennent de lecture croisée et de mesure
sur les fichiers de données. Le P0-4 (démo) mériterait une confirmation runtime de 30 s.
