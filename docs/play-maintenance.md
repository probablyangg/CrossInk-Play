# Maintaining CrossInk Play

Use the GitHub actions below to check for updates and produce an X4 Pro firmware
download. GitHub supplies the compiler and build environment.

| Task | Link |
| --- | --- |
| Build and test a firmware package | [Play - Build firmware](https://github.com/probablyangg/CrossInk-Play/actions/workflows/play-build.yml) |
| Check both upstream projects | [Play - Check upstream updates](https://github.com/probablyangg/CrossInk-Play/actions/workflows/play-upstream.yml) |
| Publish a download | [Play - Publish download](https://github.com/probablyangg/CrossInk-Play/actions/workflows/play-release.yml) |
| Request another game | [Game request form](https://github.com/probablyangg/CrossInk-Play/issues/new?template=play-game-request.yml) |
| Find published downloads | [Releases](https://github.com/probablyangg/CrossInk-Play/releases) |

## Build a download

1. Open **Play - Build firmware** and select **Run workflow**.
2. Leave the branch set to **main** to build the reviewed code, then run it.
3. Open the run. After all checks pass, download its
   **crossink-play-x4-pro-SHORTSHA** artifact from the **Artifacts** section.

The artifact contains the firmware package ZIP, the application image, checksums,
`build-info.json`, and `validation.json`. GitHub wraps an artifact download in
another ZIP; extract that download to reach the firmware package. The
12-character SHA identifies the source revision that was built. Build logs and
simulator captures are in the separate **play-validation** artifact.

The build runs the maintenance-tool tests, native game tests, reader CMake/CTest
suite, X4 Pro firmware build, package validation, and simulator journeys through
Home, Play, and the reader. A failed check blocks a successful build. Desktop
checks do not establish physical e-ink, touch, sleep, battery, or radio behavior; see
[Validation](play-validation.md).

## Keep up with CrossInk and CrossPlay

The upstream workflow is scheduled daily. For an immediate check:

1. Open **Play - Check upstream updates** and select **Run workflow** on **main**.
2. Leave **source** set to **both**, or select just **crossink** or **crossplay**.
3. Read the completed run's summary and any draft pull requests it links to.

The workflow compares stable releases with the exact versions recorded in
[`maintenance/upstreams.json`](../maintenance/upstreams.json).

| Result | Meaning |
| --- | --- |
| `current` | The recorded version matches the latest stable release. No update PR is needed. |
| `prepared_code` | A draft PR contains the proposed source changes and its updated version record. Review and test the branch before merging. |
| `needs_adaptation` | A draft PR contains an update report. Conflicts, new games, runtime/SDK changes, or protected integration files need review and adaptation first. Merging the report alone does not install the upstream update. |

CrossInk updates use a Git merge, preserving the reader's upstream history.
CrossPlay updates apply changes only to the imported games, their shared support,
assets, and mapped tests. New games and changes outside that imported scope are
reported for integration. The workflow never merges its own PRs or flashes a
reader.

### Finish an update

Open the draft PR and read its description. Complete any adaptation it identifies,
then run **Play - Build firmware** with the PR's branch selected. GitHub may also
show **Approve workflows to run** on a PR created by the bot; approve those checks
when reviewing that update.

After code review and passing checks, mark the PR ready and merge it. Use a merge
commit for a CrossInk update so its upstream ancestry remains available for the
next update. The version record on `main` advances only when the update PR is
merged. A draft branch does not change the firmware installed on the reader.

## Add another game

Open the **Game request form** and enter the game name, a source or rules link,
and how it should play using the X4 Pro's touch screen and buttons.

The form creates an implementation request. A game still needs to be written or
ported into the firmware, registered in Play, and tested. New CrossPlay games
reported by the update checker use the same process. Supported puzzle packs for
an existing game are data and can be updated independently; see
[Games and data](play-games.md).

After the game implementation is merged, **Build firmware** produces a package
containing it.

## Publish a download

Open **Play - Publish download**, select **Run workflow**, and use **main**.
The workflow rebuilds and tests that exact source revision before publishing a
prerelease under a tag such as `play-v0.1.0-alpha.1-SHORTSHA`.

Published files are available from **Releases** without opening a workflow run.
The source suffix distinguishes builds even when the firmware version has not
changed. An existing release is never overwritten. The prerelease status does
not claim physical-device validation.

## Install the new build

After the initial Play partition layout is installed, copy the compatible
**`-app.bin`** to the SD card and select
**Settings → System → SD Card Firmware Update** on the X4 Pro.

The first installation needs USB. The package's **`-usb.bin`** is for USB flashing
and must not be selected in the SD updater. See
[Installation](play-installation.md) for the complete procedure.

After an update, check the book's saved position, open the changed game, test its
save and sleep/wake behavior, dismiss the frontlight panel, then return to the
book. Test nearby multiplayer on two compatible devices when that code changes.

## One-time GitHub settings

These settings apply to the repository that runs the workflows:

- In **Settings → General → Features**, enable **Issues** to make the game request
  and bug report forms available.
- If **Actions** shows a disabled-workflows banner, enable workflows for the fork.
- In **Settings → Actions → General → Workflow permissions**, enable
  **Allow GitHub Actions to create and approve pull requests** so the update
  workflow can create its drafts. The workflow uses this permission to create
  PRs; it does not approve or merge them.

The workflows declare their own token permissions and use GitHub's built-in
`GITHUB_TOKEN`. A personal access token or custom release secret is not required.
The update workflow prepares changes without running imported firmware code;
build and test jobs use a read-only repository token.

Upstream publishing and deployment workflows are kept as inactive
[reference files](upstream-workflows/README.md). The existing CI checks remain
active alongside the Play workflows.

## Maintainer reference

The retained CI workflow runs reader static analysis on all four original
CrossInk board profiles, with all severity gates retained. Its source filter
excludes the imported game implementation; Play integration reached through
reader code is still checked. The Play workflow validates games with native
suites, the X4 Pro build, and simulator journeys. Imported game cppcheck findings
from the first scan are recorded in the [workflow reference](upstream-workflows/README.md);
passing the Play workflow does not mean that game code is cppcheck-clean.

The source manifest records the imported versions, dependency pins, and
CrossPlay path mappings. Keep it accurate when manually importing a fix. An
update to one game does not mean every game has been upgraded to a newer
CrossPlay release.

The same check can run from a development checkout:

```sh
python3 scripts/play_upstream.py check --output-dir /tmp/crossink-play-check
```

It writes `check.json` and `check.md`. Preparing one source uses:

```sh
python3 scripts/play_upstream.py prepare --source crossink --output-dir /tmp/crossink-play-update
```

Use `--source crossplay` for the game import. Preparation writes `prepare.json`
and `prepare.md`; `--publish` additionally pushes its update branch and creates
the draft PR when authenticated. Use the GitHub workflow for that authenticated
path without local token setup.

For a new native game, follow the existing
[`MinesweeperActivity`](../src/apps_local/minesweeper/MinesweeperActivity.h),
add its files under `src/apps_local/<game>/`, and register its factory in
[`Shelf.cpp`](../src/apps_local/Shelf.cpp). Add rule/save tests under
`host-tests-play/<game>/`, extend the shelf layout tests, and update
`include/PlayVersion.h`, the changelog, game documentation, and licenses.

The SDK and simulator patchers intentionally reject an unexpected dependency
revision. When an upstream update changes those dependencies, adapt the patches
or remove them if the new dependency provides the required API. Keep the source
checks in place. A fresh integration checkout avoids mixing an update with the
SDK working-tree changes created by a previous build.
