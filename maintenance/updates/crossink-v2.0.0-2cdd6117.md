# crossink update: v2.0.0

Status: **needs_adaptation**

Imported baseline: `v1.6.1` (`9914146eeae7b46b300f475a16c32426fc02ec1f`).
Candidate: [v2.0.0](https://github.com/uxjulia/CrossInk/releases/tag/v2.0.0) (`2cdd6117568c8877c3180d5fac8d5b20e55d1ac7`).

[Upstream comparison](https://github.com/uxjulia/CrossInk/compare/9914146eeae7b46b300f475a16c32426fc02ec1f...2cdd6117568c8877c3180d5fac8d5b20e55d1ac7)

**Report only: this PR does not contain an applied firmware update.**
The imported baseline is unchanged. Resolve the items below before treating this as an update.

## Needs adaptation

- `"Upstream changes maintenance/workflow control files: .github/ISSUE_TEMPLATE/bug_report.yml, .github/actions/setup-platformio/action.yml, .github/workflows/ci.yml, .github/workflows/publish-language-assets.yml, .github/workflows/release.yml, .github/workflows/release_candidate.yml"`
- `"The pinned FreeInk SDK changed; its checked Play patch needs adaptation."`

## Imported paths

- `".gitattributes"`
- `".github/ISSUE_TEMPLATE/bug_report.yml"`
- `".github/actions/setup-platformio/action.yml"`
- `".github/workflows/ci.yml"`
- `".github/workflows/publish-language-assets.yml"`
- `".github/workflows/release.yml"`
- `".github/workflows/release_candidate.yml"`
- `".gitignore"`
- `"AGENTS.md"`
- `"CHANGELOG.md"`
- `"LICENSE"`
- `"README.md"`
- `"assets/hyphenation-extra/README.md"`
- `"assets/hyphenation-extra/licenses/LPPL-1.3.txt"`
- `"assets/hyphenation-extra/licenses/LPPL-1.3c.txt"`
- `"assets/hyphenation-extra/packs.json"`
- `"assets/hyphenation-extra/patterns/hyph-bg.tex"`
- `"assets/hyphenation-extra/patterns/hyph-ca.tex"`
- `"assets/hyphenation-extra/patterns/hyph-cs-sojka.tex"`
- `"assets/hyphenation-extra/patterns/hyph-da.tex"`
- `"assets/hyphenation-extra/patterns/hyph-fi-x-school.tex"`
- `"assets/hyphenation-extra/patterns/hyph-gl.tex"`
- `"assets/hyphenation-extra/patterns/hyph-hr.tex"`
- `"assets/hyphenation-extra/patterns/hyph-is.tex"`
- `"assets/hyphenation-extra/patterns/hyph-kmr.tex"`
- `"assets/hyphenation-extra/patterns/hyph-la.tex"`
- `"assets/hyphenation-extra/patterns/hyph-lt.tex"`
- `"assets/hyphenation-extra/patterns/hyph-nl.tex"`
- `"assets/hyphenation-extra/patterns/hyph-no.tex"`
- `"assets/hyphenation-extra/patterns/hyph-sl.tex"`
- `"assets/hyphenation-extra/patterns/hyph-sq.tex"`
- `"assets/hyphenation-extra/tries/bg.bin"`
- `"assets/hyphenation-extra/tries/ca.bin"`
- `"assets/hyphenation-extra/tries/cs.bin"`
- `"assets/hyphenation-extra/tries/da.bin"`
- `"assets/hyphenation-extra/tries/fi.bin"`
- `"assets/hyphenation-extra/tries/gl.bin"`
- `"assets/hyphenation-extra/tries/hr.bin"`
- `"assets/hyphenation-extra/tries/is.bin"`
- `"assets/hyphenation-extra/tries/ku.bin"`
- `"assets/hyphenation-extra/tries/la.bin"`
- `"assets/hyphenation-extra/tries/lt.bin"`
- `"assets/hyphenation-extra/tries/nl.bin"`
- `"assets/hyphenation-extra/tries/no.bin"`
- `"assets/hyphenation-extra/tries/sl.bin"`
- `"assets/hyphenation-extra/tries/sq.bin"`
- `"assets/hyphenation/LICENSE-APACHE"`
- `"assets/hyphenation/LICENSE-MIT"`
- `"assets/hyphenation/README.md"`
- `"assets/hyphenation/patterns/hyph-de-1996.tex"`
- `"assets/hyphenation/patterns/hyph-es.tex"`
- `"assets/hyphenation/patterns/hyph-fr.tex"`
- `"assets/hyphenation/patterns/hyph-it.tex"`
- `"assets/hyphenation/patterns/hyph-pl.tex"`
- `"assets/hyphenation/patterns/hyph-pt.tex"`
- `"assets/hyphenation/patterns/hyph-ru.tex"`
- `"assets/hyphenation/patterns/hyph-sv.tex"`
- `"assets/hyphenation/patterns/hyph-uk.tex"`
- `"bin/crossink-emulator.command"`
- `"docs/controls.md"`
- `"docs/data-cache.md"`
- `"docs/development/README.md"`
- `"docs/development/architecture.md"`
- `"docs/development/eego-a4-hardware.md"`
- `"docs/development/font-development.md"`
- `"docs/development/getting-started.md"`
- `"docs/development/picco-hardware.md"`
- `"docs/development/plugin-development.md"`
- `"docs/development/testing-debugging.md"`
- `"docs/dictionary-development.md"`
- `"docs/dictionary.md"`
- `"docs/epub-indexing.md"`
- `"docs/epub-render-modes.md"`
- `"docs/file-formats.md"`
- `"docs/index.md"`
- `"docs/installation.md"`
- `"docs/languages.md"`
- `"docs/nearby-file-transfer.md"`
- `"docs/nearby-position-sync.md"`
- `"docs/picco.md"`
- `"docs/reader-features.md"`
- `"docs/reading-stats-sync.md"`
- `"docs/scalable-fonts.md"`
- `"docs/sd-card-fonts.md"`
- `"docs/sd-plugins.md"`
- `"docs/touch-navigation.md"`
- `"docs/translators.md"`
- `"docs/troubleshooting.md"`
- `"docs/user-guide.md"`
- `"docs/webserver-endpoints.md"`
- `"docs/webserver.md"`
- `"freeink-sdk"`
- `"include/CrossInkHalFrontlight.h"`
- `"include/DeviceCapabilities.h"`
- `"include/SimulatorContentCrypto.h"`
- `"include/TiltSensitivity.h"`
- `"include/WifiTransmitPower.h"`
- `"lib/AppVersion/AppVersion.h"`
- `"lib/AssetCacheStorage/AssetCacheReadSession.h"`
- `"lib/AssetCacheStorage/AssetCacheStorage.cpp"`
- `"lib/AssetCacheStorage/AssetCacheStorage.h"`
- `"lib/AssetCatalog/AssetCatalog.cpp"`
- `"lib/AssetCatalog/AssetCatalog.h"`
- `"lib/AssetCompanionIndex/AssetCompanionIndex.cpp"`
- `"lib/AssetCompanionIndex/AssetCompanionIndex.h"`
- `"lib/BmpToBmpConverter/BmpToBmpConverter.cpp"`
- `"lib/BmpToBmpConverter/BmpToBmpConverter.h"`
- `"lib/DeviceInfo/HalDeviceIdentity.cpp"`
- `"lib/DeviceInfo/HalDeviceInfo.cpp"`
- `"lib/DeviceInfo/HalDeviceInfo.h"`
- `"lib/DeviceSecret/DeviceSecret.cpp"`
- `"lib/DeviceSecret/DeviceSecret.h"`
- `"lib/EpdFont/EpdFont.cpp"`
- `"lib/EpdFont/EpdFont.h"`
- `"lib/EpdFont/EpdFontData.h"`
- `"lib/EpdFont/EpdFontFamily.cpp"`
- `"lib/EpdFont/EpdFontFamily.h"`
- `"lib/EpdFont/FontCatalogIndex.h"`
- `"lib/EpdFont/FontDecompressor.cpp"`
- `"lib/EpdFont/FontDecompressor.h"`
- `"lib/EpdFont/FontFileTypes.h"`
- `"lib/EpdFont/SdCardFont.cpp"`
- `"lib/EpdFont/SdCardFont.h"`
- `"lib/EpdFont/SdCardFontManager.cpp"`
- `"lib/EpdFont/SdCardFontManager.h"`
- `"lib/EpdFont/SdCardFontRegistry.cpp"`
- `"lib/EpdFont/builtinFonts/all.h"`
- `"lib/EpdFont/builtinFonts/all_faces.h"`
- `"lib/EpdFont/builtinFonts/inter_10_bold.h"`
- `"lib/EpdFont/builtinFonts/inter_10_regular.h"`
- `"lib/EpdFont/builtinFonts/inter_12_bold.h"`
- `"lib/EpdFont/builtinFonts/inter_12_regular.h"`
- `"lib/EpdFont/builtinFonts/inter_8_regular.h"`
- `"lib/EpdFont/scripts/convert-builtin-fonts.sh"`
- `"lib/EpdFont/scripts/fontconvert_sdcard.py"`
- `"lib/EpdFont/scripts/share_builtin_tables.py"`
- `"lib/Epub/BookKey.cpp"`
- `"lib/Epub/BookKey.h"`
- `"lib/Epub/ContentProtection.cpp"`
- `"lib/Epub/Epub.cpp"`
- `"lib/Epub/Epub.h"`
- `"lib/Epub/Epub/BookMetadataCache.cpp"`
- `"lib/Epub/Epub/BookMetadataCache.h"`
- `"lib/Epub/Epub/EpubRenderMode.h"`
- `"lib/Epub/Epub/Page.cpp"`
- `"lib/Epub/Epub/Page.h"`
- `"lib/Epub/Epub/ParsedText.cpp"`
- `"lib/Epub/Epub/ParsedText.h"`
- `"lib/Epub/Epub/ReaderRenderSpec.h"`
- `"lib/Epub/Epub/ReferencePageNavigation.h"`
- 1300 additional paths are listed in the JSON report.

## Before merging

- Review local Play adapters, reader integration, save formats, and dependency changes.
- Run Play - Build firmware on the actual PR branch; review the package and test output.
- Check the X4 Pro's reader/game/reader journey, saves, frontlight, and sleep/wake.
- Keep CrossInk merge ancestry when merging a CrossInk code-update PR.

This automation never merges PRs, publishes firmware releases, or flashes a device.
