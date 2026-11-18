# crossplay update: v1.15.1

Status: **needs_adaptation**

Imported baseline: `v1.14.2` (`0a1d17f41e1605d3b033d4327c81b3bcfb2ca2ab`).
Candidate: [v1.15.1](https://github.com/ma-r-s/crossplay/releases/tag/v1.15.1) (`93faa4971b879a6a0cea4a5965c3c2cd6e8ca04f`).

[Upstream comparison](https://github.com/ma-r-s/crossplay/compare/0a1d17f41e1605d3b033d4327c81b3bcfb2ca2ab...93faa4971b879a6a0cea4a5965c3c2cd6e8ca04f)

**Report only: this PR does not contain an applied firmware update.**
The imported baseline is unchanged. Resolve the items below before treating this as an update.

## Needs adaptation

- `"Upstream runtime or dependency changes require compatibility review: freeink-sdk, lib/GfxRenderer/Bitmap.cpp, lib/GfxRenderer/Bitmap.h, lib/GfxRenderer/FontCacheManager.cpp, lib/GfxRenderer/FontCacheManager.h, lib/GfxRenderer/FontVariantProvider.h, lib/GfxRenderer/GfxRenderer.cpp, lib/GfxRenderer/GfxRenderer.h, lib/hal/HalStorage.cpp, lib/hal/HalStorage.h, platformio.ini, src/MappedInputManager.cpp, src/MappedInputManager.h, src/activities/Activity.h, src/activities/ActivityManager.cpp, src/activities/ActivityManager.h, src/activities/ActivityResult.h, src/main.cpp"`
- `"Unmapped shared game-layer files need review: src/apps_local/ClockFloor.cpp, src/apps_local/ClockFloor.h"`

## Imported paths

- `"src/apps_local/LICENSE.CrossPlay"`
- `"src/apps_local/THIRD-PARTY-NOTICES.CrossPlay.md"`
- `"src/apps_local/Shelf.cpp"`
- `"src/apps_local/connections/ConnectionsActivity.cpp"`
- `"src/apps_local/solitaire/SolitaireScreens.cpp"`
- `"src/apps_local/solitaire/SolitaireScreens.h"`
- `"src/apps_local/sudoku/SudokuScreens.cpp"`
- `"src/apps_local/sudoku/SudokuScreens.h"`
- `"src/apps_local/ui/ToyboxFonts.cpp"`
- `"src/apps_local/ui/ToyboxIcons.h"`
- `"src/apps_local/ui/ToyboxScreen.h"`
- `"src/apps_local/ui/ToyboxTokens.h"`
- `"src/apps_local/wavelength/WavelengthActivity.cpp"`
- `"src/apps_local/wordle/WordleActivity.cpp"`

## Changes outside the game import

- `".agents/README.md"`
- `".agents/rules/architecture-hal.md"`
- `".agents/rules/coding-standards.md"`
- `".agents/rules/environment.md"`
- `".agents/rules/generated-files-cache.md"`
- `".agents/rules/git-workflow.md"`
- `".agents/rules/hardware-resources.md"`
- `".agents/rules/testing-debugging.md"`
- `".agents/rules/ui-activities.md"`
- `".agents/skills/firmware-handoff/SKILL.md"`
- `".agents/skills/hal-and-abstractions/SKILL.md"`
- `".agents/skills/heap-discipline/SKILL.md"`
- `".agents/skills/refactor-for-review/SKILL.md"`
- `".agents/skills/review-architecture/SKILL.md"`
- `".agents/skills/review-correctness/SKILL.md"`
- `".agents/skills/review-embedded/SKILL.md"`
- `".agents/skills/review-i18n-docs/SKILL.md"`
- `".gitattributes"`
- `".github/scripts/firmware-size-comparison.js"`
- `".github/scripts/firmware-stats.py"`
- `".github/scripts/pr-firmware-comment.js"`
- `".github/workflows/ci.yml"`
- `".github/workflows/crossplay-ci.yml"`
- `".github/workflows/pr-firmware-links.yml"`
- `".github/workflows/release_candidate.yml"`
- `".gitignore"`
- `"LOCAL_SCOPE.md"`
- `"README.md"`
- `"SCOPE.md"`
- `"USER_GUIDE.md"`
- `"docs/README.md"`
- `"docs/apps/README.md"`
- `"docs/apps/notes-design.md"`
- `"docs/apps/rss.md"`
- `"docs/apps/study-anki-compatibility.md"`
- `"docs/apps/study-deck-format.md"`
- `"docs/apps/study-quick-reference.pdf"`
- `"docs/apps/study.md"`
- `"docs/apps/wikipedia-plan.md"`
- `"docs/building-apps.md"`
- `"docs/buttons.md"`
- `"docs/contributing/architecture.md"`
- `"docs/contributing/touch-and-ui.md"`
- `"docs/controls.md"`
- `"docs/dictionary.md"`
- `"docs/file-formats.md"`
- `"docs/i18n.md"`
- `"docs/install.md"`
- `"docs/picco.md"`
- `"docs/plugin-events.md"`
- `"docs/release-body.md"`
- `"docs/release-notes.md"`
- `"docs/sd-card-fonts.md"`
- `"docs/sd-plugins.md"`
- `"docs/translators.md"`
- `"docs/webserver-endpoints.md"`
- `"docs/webserver.md"`
- `"freeink-sdk"`
- `"host-tests/appsettings/test_appsettings.py"`
- `"host-tests/appslot/run.sh"`
- `"host-tests/appslot/test_appslot.py"`
- `"host-tests/brand/run.sh"`
- `"host-tests/checksh/run.sh"`
- `"host-tests/clockfloor/run.sh"`
- `"host-tests/clockfloor/test_clockfloor.cpp"`
- `"host-tests/devinput/stubs/InputManager.h"`
- `"host-tests/fittedtitle/corpus.py"`
- `"host-tests/headerrow/run.sh"`
- `"host-tests/headerrow/test_header_row.py"`
- `"host-tests/httpdoor/door.py"`
- `"host-tests/httpdoor/run.sh"`
- `"host-tests/live/run.sh"`
- `"host-tests/notes/run.sh"`
- `"host-tests/picco/run.sh"`
- `"host-tests/picco/stubs/esp_ota_ops.h"`
- `"host-tests/picco/test_config.py"`
- `"host-tests/picco/test_profile.cpp"`
- `"host-tests/picco/test_updateslot.cpp"`
- `"host-tests/prewarmscope/test_prewarmscope.cpp"`
- `"host-tests/release/run.sh"`
- `"host-tests/rss/run.sh"`
- `"host-tests/rss/test_rss.cpp"`
- `"host-tests/settingsorder/test_settingsorder.py"`
- `"host-tests/site/install_fn.js"`
- `"host-tests/site/report_fn.js"`
- `"host-tests/site/run.sh"`
- `"host-tests/study/make_fixture.py"`
- `"host-tests/study/run.sh"`
- `"host-tests/study/test_page_keys.py"`
- `"host-tests/study/test_paging.cpp"`
- `"host-tests/study/test_stats.cpp"`
- `"host-tests/tilefit/test_tilefit.cpp"`
- `"host-tests/typefold/test_typefold.cpp"`
- `"host-tests/ui/run.sh"`
- `"host-tests/ui/test_ui.cpp"`
- `"host-tests/usbflash/run.sh"`
- `"host-tests/wakepolicy/run.sh"`
- `"host-tests/wallcaption/test_wallcaption.cpp"`
- `"host-tests/wikipedia/run.sh"`
- `"host-tests/wikipedia/test_install_face.py"`
- `"lib/AsciiText/AsciiText.h"`
- `"lib/BlePageTurner/include/BleKeyBinding.h"`
- `"lib/BlePageTurner/include/BlePageTurner.h"`
- `"lib/BlePageTurner/include/BlePageTurnerJson.h"`
- `"lib/BlePageTurner/library.json"`
- `"lib/BlePageTurner/src/NoRadio.cpp"`
- `"lib/BlePageTurner/src/RadioPolicy.h"`
- `"lib/BlePageTurner/src/RadioPort.h"`
- `"lib/BlePageTurner/src/Runtime.cpp"`
- `"lib/BlePageTurner/src/Runtime.h"`
- `"lib/BlePageTurner/src/SdkRadio.cpp"`
- `"lib/BlePageTurner/test/CMakeLists.txt"`
- `"lib/BlePageTurner/test/ConfigJsonTest.cpp"`
- `"lib/BlePageTurner/test/Fakes.cpp"`
- `"lib/BlePageTurner/test/Fakes.h"`
- `"lib/BlePageTurner/test/HeapRestartTest.cpp"`
- `"lib/BlePageTurner/test/HomeStartTest.cpp"`
- `"lib/BlePageTurner/test/KeyBindingTest.cpp"`
- `"lib/BlePageTurner/test/LinkNoteTest.cpp"`
- `"lib/BlePageTurner/test/NoRadioTest.cpp"`
- `"lib/BlePageTurner/test/RadioVerdictS3Test.cpp"`
- `"lib/BlePageTurner/test/RadioVerdictTest.cpp"`
- `"lib/BlePageTurner/test/RequestConnectTest.cpp"`
- `"lib/BlePageTurner/test/RuntimeTest.cpp"`
- `"lib/BlePageTurner/test/SourceContractTest.cpp"`
- `"lib/BlePageTurner/test/TickTest.cpp"`
- `"lib/BmpToBmpConverter/BmpToBmpConverter.cpp"`
- `"lib/BmpToBmpConverter/BmpToBmpConverter.h"`
- `"lib/DevInput/DevInputCommands.cpp"`
- `"lib/EpdFont/EpdFont.cpp"`
- `"lib/EpdFont/EpdFontData.h"`
- `"lib/EpdFont/EpdFontFamily.h"`
- `"lib/EpdFont/FontDecompressor.h"`
- `"lib/EpdFont/SdCardFont.cpp"`
- `"lib/EpdFont/SdCardFont.h"`
- `"lib/EpdFont/SdCardFontManager.cpp"`
- `"lib/EpdFont/SdCardFontManager.h"`
- `"lib/EpdFont/SdCardFontRegistry.cpp"`
- `"lib/EpdFont/SdCardFontRegistry.h"`
- `"lib/EpdFont/TtfEpdFont.cpp"`
- `"lib/EpdFont/TtfEpdFont.h"`
- `"lib/EpdFont/builtinFonts/README.md"`
- `"lib/EpdFont/builtinFonts/all.h"`
- `"lib/EpdFont/builtinFonts/notosans_8_regular.h"`
- `"lib/EpdFont/builtinFonts/notoserif_12_bold.h"`
- `"lib/EpdFont/builtinFonts/notoserif_12_bolditalic.h"`
- `"lib/EpdFont/builtinFonts/notoserif_12_italic.h"`
- `"lib/EpdFont/builtinFonts/notoserif_12_regular.h"`
- `"lib/EpdFont/builtinFonts/notoserif_14_bold.h"`
- `"lib/EpdFont/builtinFonts/notoserif_14_bolditalic.h"`
- 566 additional paths are listed in the JSON report.

## Before merging

- Review local Play adapters, reader integration, save formats, and dependency changes.
- Run Play - Build firmware on the actual PR branch; review the package and test output.
- Check the X4 Pro's reader/game/reader journey, saves, frontlight, and sleep/wake.
- Keep CrossInk merge ancestry when merging a CrossInk code-update PR.

This automation never merges PRs, publishes firmware releases, or flashes a device.
