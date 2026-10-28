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
its formatting, static-analysis, and additional board-build checks.
