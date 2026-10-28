# Upstream workflow reference

These files were inherited from CrossInk and are retained unchanged for reference.
GitHub does not run workflows from this directory.

| Archived workflow | Reason |
| --- | --- |
| `release.yml`, `release_candidate.yml` | Publish upstream board images using CrossInk-specific secrets. Play uses [Publish download](../../.github/workflows/play-release.yml) for its tested X4 Pro package. |
| `release-fonts.yml` | Publishes to CrossInk's font repository and S3 storage. |
| `pages.yml` | Deploys the inherited website when documentation changes. |
| `issue-triage.yml` | Dispatches bug reports to CrossInk's separate triage repository. |

The active Play workflows check upstream releases, build and test firmware, and
publish downloads. See [Easy maintenance](../play-maintenance.md).

The inherited [CI workflow](../../.github/workflows/ci.yml) remains active to retain
its formatting, static-analysis, and additional board-build checks. Static
analysis explicitly checks the reader code on all four original CrossInk
profiles (`default`, `sticky`, `x4-pro`, and `x4-classic`), including the Play
integration reached from that code, with the existing low, medium, and high
failure thresholds. Its source filter excludes the imported implementation in
`src/apps_local/`; game native suites, firmware builds, and simulator checks run
in the Play workflow. This is not a claim that imported games pass cppcheck.
The [first unscoped scan](https://github.com/probablyangg/CrossInk-Play/actions/runs/40634053446)
reported 7 medium and 395 low diagnostics in the imported game/runtime layer,
with no high diagnostics; these findings are separate from the reader check.
