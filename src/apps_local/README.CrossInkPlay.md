# CrossInk Play import

The game layer comes from `ma-r-s/crossplay` tag `v1.14.2` and extends CrossInk
`v1.6.1`. The CrossInk reader and its pinned FreeInk SDK remain the base.

`Shelf.cpp` registers all 25 games in one Play folder. CrossInk Home opens it
with `shelf::openFolder(0, renderer, mappedInput)`. Player names, avatars,
multiplayer, game saves and optional puzzle/question downloads remain available.
The imported save paths and data formats are retained. Trivia stores its local
US-question toggle in an optional second byte of `/trivia/prefs`; older
one-byte difficulty files still load.

## Adaptations

- `ui/ToyboxScreen.h` supplies game-local equivalents of the two newer SDK
  layout operations, keeping CrossInk's SDK revision unchanged.
- The optional CrossPlay developer server and its radio coordination are not
  included. Multiplayer still owns and releases the Wi-Fi radio for ESP-NOW.
- Trivia can hide/restore questions and fetch/check its packs. It does not
  submit reports or device analytics. Existing report queue files are untouched.
- The Play heading uses CrossInk I18n. Imported game text retains the upstream
  English wording.
- Host checks are isolated in `host-tests-play/`, separate from CrossInk tests.
  Developer terrain-editor and Underhand seed-tool checks are outside this
  import; the game rules, AI, save, pack, multiplayer and layout suites remain.

## Licenses and source attribution

`LICENSE.CrossPlay` and `THIRD-PARTY-NOTICES.CrossPlay.md` retain the upstream
notices. The latter describes the full upstream distribution, including apps
and websites absent from this fork. Relevant full license texts are retained:

- Jersey 25, Instrument Serif and Noto Serif: `ui/fonts/OFL-*.txt`.
- Noto Sans Symbols 2: `solitaire/art/OFL.txt`.
- Celtic chess pieces: repository `assets_local/pieces/celtic/LICENSE` and SVGs.
- ArduGo pattern derivation: repository `assets_local/go/LICENSE-ardugo`.
- Michi source grant: `go/michi/UPSTREAM-README.md` and source comments.
- Lucide icons: repository `freeink-sdk/libs/assets/Icons/lucide/LICENSE`.

The game names and downloaded content retain their upstream ownership and
attribution as described in the notices. This fork makes no affiliation claim.
