# crossplay update: v1.14.4

Status: **prepared_code**

Imported baseline: `v1.14.2` (`0a1d17f41e1605d3b033d4327c81b3bcfb2ca2ab`).
Candidate: [v1.14.4](https://github.com/ma-r-s/crossplay/releases/tag/v1.14.4) (`5f02702301462e0ac0d6b25244ea43fd5f68dd5d`).

[Upstream comparison](https://github.com/ma-r-s/crossplay/compare/0a1d17f41e1605d3b033d4327c81b3bcfb2ca2ab...5f02702301462e0ac0d6b25244ea43fd5f68dd5d)

The candidate contains code changes and a proposed source-baseline update.
It remains a draft for compatibility review; a clean Git operation is not a passed firmware test.

## Imported paths

- `"src/apps_local/solitaire/SolitaireScreens.cpp"`
- `"src/apps_local/solitaire/SolitaireScreens.h"`
- `"src/apps_local/sudoku/SudokuScreens.cpp"`
- `"src/apps_local/sudoku/SudokuScreens.h"`

## Changes outside the game import

- `"docs/release-body.md"`
- `"docs/release-notes.md"`
- `"host-tests/release/run.sh"`
- `"host-tests/ui/test_ui.cpp"`
- `"platformio.ini"`
- `"scripts_local/release_notes.py"`

## Before merging

- Review local Play adapters, reader integration, save formats, and dependency changes.
- Run Play - Build firmware on the actual PR branch; review the package and test output.
- Check the X4 Pro's reader/game/reader journey, saves, frontlight, and sleep/wake.
- Keep CrossInk merge ancestry when merging a CrossInk code-update PR.

This automation never merges PRs, publishes firmware releases, or flashes a device.
