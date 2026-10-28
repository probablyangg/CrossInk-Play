# CrossInk Play games

CrossInk Play adds a **Play** entry to CrossInk's Home screen and brings over
all 25 games registered by CrossPlay v1.14.2. It is based on CrossInk v1.6.1
and targets the touch-enabled Xteink X4 Pro. Reading, the library, typography
and normal device settings continue to use CrossInk's existing activities.

This is a firmware source integration. It does not load a second firmware from
an SD-card folder. Build and installation instructions are in the fork's main
documentation.

## Opening and leaving Play

Open **Play** from Home, then tap a game. The side page keys, the page marks and
vertical swipes move between pages of games. The game shelf remembers the page
you last used. Tap the shelf heading to choose which games are visible; use
**Done** to return to the normal list. Hidden games remain installed and can be
shown again from the same chooser.

The player name and avatar at the bottom of the shelf open **Player**. Those
choices are shared by games that support nearby play. A game's Back action
returns through its own menus to Play; Back from Play returns to CrossInk Home.
The normal Home gesture remains available. Some games use landscape during
play and restore the reader's orientation on exit.

The Play heading uses CrossInk's translation system. The imported games retain
CrossPlay's English menus and instructional text.

## Included games

“Nearby” means two compatible devices using the retained CrossPlay multiplayer
protocol. “One device” party games pass or share a single reader and need no
radio. The path column gives the main persistent files; the paths are relative
to the SD card's `/.crosspoint/` directory unless shown with a leading `/`.

| Game | Play available | Main saved files |
| --- | --- | --- |
| Chess | Computer opponent; nearby opponent | `chess.sav`, `chess.cfg` |
| Battleship | Computer opponent; nearby opponent | `bship.sav`, `bship.cfg` |
| Connections | Word grouping puzzles from an imported archive | `connections.sav`, `connections.res`, `connections.idx`, `connections.dat` |
| Solitaire | Solo card game | `solitaire.sav`, `solitaire.res` |
| Hearts | Solo against computer players | `hearts.sav`, `hearts.skill`, `hearts.res` |
| D&Diagrams | Solo dungeon logic puzzles | `dungeon.sav` |
| Insider | Secret-role party game for four to eight people, one device | `insider.sav` |
| Jaipur | Computer opponent; nearby opponent | `jaipur.sav`, `jaipur.stats` |
| Sea Salt | Computer opponent; nearby opponent | `seasalt.sav`, `seasalt.stats` |
| Murdle | Solo deduction puzzles | `murdle.sav` |
| Checkers | Computer opponent; nearby opponent | `checkers.sav` |
| Connect Four | Computer opponent; nearby opponent | `connectfour.sav` |
| Yahtzee | Computer opponent; nearby opponent | `yahtzee.sav` |
| Knucklebones | Computer opponent; nearby opponent | `knucklebones.sav` |
| Minesweeper | Solo mine-clearing puzzle | `minesweeper.sav` |
| Sudoku | Solo number puzzles | `sudoku.sav` |
| Picross | Solo picture logic puzzles | `picross.sav` |
| Toy Battle | Computer opponent; nearby opponent | `toybattle.sav`, `toybattle.stats` |
| Forehead | Guessing party game, one device | `forehead.sav` |
| Trivia | Solo multiple choice or read-aloud quizmaster mode | `/trivia/pack.dat`, `/trivia/pack.state`, `/trivia/pack.meta`, `/trivia/prefs` |
| Wavelength | Shared-device clue and spectrum party game | `wavelength.sav` |
| Go | Computer opponent using the imported Michi engine; nearby opponent | `go.sav` |
| Hex | Computer opponent; nearby opponent | `hex.sav` |
| Underhand | Solo card and resource game | `underhand.sav` |
| Wordle | Daily and archive word puzzles from imported word data | `wordle.ans`, `wordle.words`, `wordle.res`, `wordle.games` |

Game rules, opponents, tutorials, selectors, saves and result histories are
retained from the upstream game directories. The shelf also retains the shared
card art, fonts, player avatars and nearby-game screens.

## Games that download data

Connections, Wordle and Trivia require their data files to exist on the SD card.
A card that already contains compatible CrossPlay data can supply those files;
a fresh card uses the game's own Wi-Fi download flow. Their data stays on the
card for subsequent offline play. The other 22 games do not need a content
download to start.

The following are the endpoints configured in the imported source. They are
external services, so an unavailable endpoint, changed response format or
network/TLS failure can prevent a fresh download even when the game itself is
installed. Offline games and already downloaded packs do not require those
servers to answer.

### Connections

The importer streams the archive from:

- `https://raw.githubusercontent.com/Eyefyre/NYT-Connections-Answers/main/connections.json`

It builds `/.crosspoint/connections.idx` and
`/.crosspoint/connections.dat`, while progress and results live in the separate
`.sav` and `.res` files. Import files are written with temporary names before
being installed. The app has a calendar/archive selector and uses the device
date for the current puzzle. The puzzles belong to their original publisher;
this firmware includes the importer, not a bundled copy of the puzzle archive.

The inherited download flow performs a quiet firmware restart when leaving
Connections after using Wi-Fi, to release the TLS session's heap allocations.
That behavior is retained in this port.

### Wordle

The initial archive and valid-word list are read from:

- `https://raw.githubusercontent.com/mfilej/wrdl/main/solutions-era1.txt`
- `https://raw.githubusercontent.com/mfilej/wrdl/main/solutions.txt`
- `https://raw.githubusercontent.com/mfilej/wrdl/main/valid.txt`

Recent daily answers can be topped up from the date-specific NYT endpoint:

- `https://www.nytimes.com/svc/wordle/v2/YYYY-MM-DD.json`

The app chooses an archive import when the local data is missing or too far
behind, and otherwise fetches recent daily entries. The data files are
`wordle.ans` and `wordle.words`; results and saved boards use `wordle.res` and
`wordle.games`. Set the device date correctly for the intended daily puzzle.
Like Connections, Wordle retains its quiet restart on exit after using Wi-Fi.

### Trivia

Trivia obtains a question pack and its version manifest from the separate
CrossPlay data release:

- `https://github.com/ma-r-s/crossplay/releases/download/trivia-pack/pack.dat`
- `https://github.com/ma-r-s/crossplay/releases/download/trivia-pack/pack.json`

On a fresh card, **Get the questions** opens Wi-Fi selection and downloads the
pack. The UI describes the upstream pack as about 6 MB; the actual published
size comes from its manifest. The download writes `/trivia/pack.dat.part`, then
installs the completed pack. Back or Home can cancel the download. With an
existing pack, **Trivia → Settings → Sync** checks the published manifest and
offers a newer or different pack when appropriate. This action checks game
data; it is unrelated to firmware OTA updates.

`pack.state` records seen and hidden questions, `pack.meta` identifies the local
build, and `prefs` stores difficulty plus the Play-owned **US questions**
toggle. A missing toggle defaults to off, matching the upstream default. An
older one-byte preference file still loads its difficulty; Play writes the
optional second byte when either preference changes. The toggle is independent
of CrossInk's general settings.

**Hide**, **Undo** and **show hidden questions again** operate locally. This
fork does not submit question reports, create an outbound report queue or send
device analytics. It leaves any existing `/trivia/reports.dat` file untouched.
The upstream “Why?” reporting prompt is absent because it would have no
recipient. Pack checks and downloads remain available.

## Nearby multiplayer

Eleven games retain nearby play: Chess, Battleship, Jaipur, Sea Salt, Checkers,
Connect Four, Yahtzee, Knucklebones, Toy Battle, Go and Hex. Open the same game
on both devices, choose its nearby/play-a-person option, select the partner
and accept the invitation.

On hardware this uses ESP-NOW on the imported channel setting (channel 1),
without an internet account or a home Wi-Fi router. A match takes ownership of
the Wi-Fi radio, ends any current access-point association and switches the
radio off when the match releases it. Reading and solo play do not require the
multiplayer radio to remain active.

The game packets, version identifiers and serialization are retained from
CrossPlay v1.14.2. Compatibility with that same upstream protocol is intended;
real-device interoperability, range and power behavior still require testing
on two physical readers. A desktop simulation uses local UDP instead of
ESP-NOW. Its native tests exercise the shared protocol and whole matches, but
cannot validate the X4 Pro's RF hardware.

## Existing data, preferences and wake behavior

Save names and formats are retained so compatible CrossPlay files can continue
to be used. This is not a promise that arbitrary future CrossPlay save formats
will be readable. Keep a backup of the SD card before switching firmware,
especially `/.crosspoint/` and `/trivia/`.

The shared Play files are:

| File | Purpose |
| --- | --- |
| `/.crosspoint/shelf.cfg` | Last folder, page/row and the game remembered for wake |
| `/.crosspoint/shelf-hidden.cfg` | Games hidden from the shelf |
| `/.crosspoint/player.cfg` | Shared player name/avatar choices |

Each game retains its own save cadence and recovery behavior. A multiplayer
session is not a durable network connection and should not be expected to
resume across a firmware restart. Game choices, saved solo progress and results
are separate from the reader's book position.

Play's sleep/wake hooks remember the game activity and reopen its normal entry
on wake; the game's existing save code restores the state it supports. A game
may return to its front menu with a saved round available rather than directly
to its play board. Leaving Play for the reader clears the active Play wake
intent. The core reader remains responsible for its own book state.

## What was adapted

The import is isolated under `src/apps_local/`, with the small Activity,
rendering, storage and input adapters needed to host it in CrossInk. The SDK is
still CrossInk's pinned FreeInk revision. Two newer layout operations used by
CrossPlay are supplied by the game-local `toybox::Screen` wrapper, so they do
not require replacing the reader's UI toolkit.

The game shelf contains one **Play** folder. CrossPlay's separate non-game
apps, developer server, optional report submission and device analytics are
outside this integration. Player profiles and all game networking/download
flows described above are retained. Generated game fonts are registered under
their own IDs when Play first needs them.

## Verification and source licenses

The isolated `host-tests-play/` suites cover imported rules, opponents,
serialization, file formats, puzzle data, multiplayer and reveal behavior.
The Play UI suite compiles all imported freestanding game screen builders
against CrossInk's pinned SDK with warnings treated as errors. Its adapter
checks cover portrait and landscape layout, header paint beneath the bezel,
visible title placement, action routing and access to all 25 shelf rows.
Runtime reveal/completion checks are in `host-tests-play/runtime/`.

Examples:

```sh
sh host-tests-play/chess/run.sh
sh host-tests-play/trivia/run.sh
sh host-tests-play/link/run.sh
sh host-tests-play/ui/run.sh
sh host-tests-play/runtime/run.sh
```

The multiplayer tests need permission to bind local UDP sockets. A sandbox
that denies socket creation fails their radio setup; it does not test the
hardware transport. Native checks and successful firmware builds do not
replace a physical X4 Pro smoke test for touch, e-paper timing, sleep, SD and
nearby play. The final build/package documentation records what was actually
built and tested for this fork.

The CrossPlay MIT license and upstream third-party notices are preserved in
`src/apps_local/LICENSE.CrossPlay` and
`src/apps_local/THIRD-PARTY-NOTICES.CrossPlay.md`. The latter describes the full
upstream distribution, including apps and web assets not imported here. The
relevant full notices travel with this source:

- Jersey 25, Instrument Serif and Noto Serif: `src/apps_local/ui/fonts/OFL-*.txt`.
- Noto Sans Symbols 2: `src/apps_local/solitaire/art/OFL.txt`.
- Celtic chess-piece artwork: `assets_local/pieces/celtic/LICENSE` and SVGs.
- ArduGo's pattern derivation: `assets_local/go/LICENSE-ardugo`.
- Michi engine grant: `src/apps_local/go/michi/UPSTREAM-README.md`.
- Lucide icons: `freeink-sdk/libs/assets/Icons/lucide/LICENSE`.

Game names and downloaded content retain their owners' rights. This fork is
not affiliated with or endorsed by the publishers of those games or datasets.
