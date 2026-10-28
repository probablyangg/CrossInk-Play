#!/usr/bin/env python3
"""Check upstream releases and prepare reviewable CrossInk Play update drafts.

Only standard-library Python and Git are required. Preparation never runs
upstream code, builds firmware, merges a PR, or contacts a reader. Candidate
changes are made in an isolated temporary worktree. Publishing uses the
repository's GITHUB_TOKEN; no personal access token is required in Actions.
"""

from __future__ import annotations

import argparse
import base64
import configparser
import copy
from datetime import datetime, timezone
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = "maintenance/upstreams.json"
UPSTREAMS = {"crossink": "uxjulia/CrossInk", "crossplay": "ma-r-s/crossplay"}
SHA_RE = re.compile(r"[0-9a-f]{40}\Z")
TAG_RE = re.compile(r"v?[0-9]+\.[0-9]+\.[0-9]+(?:[-.][A-Za-z0-9]+)*\Z")
PATH_RE = re.compile(r"[A-Za-z0-9_./+-]+\Z")
MAX_API_BYTES = 8 * 1024 * 1024
MAX_PATCH_BYTES = 32 * 1024 * 1024
CONTROL_PATHS = (
    ".github/", "maintenance/", "scripts/play_upstream.py",
    "scripts/test_play_upstream.py", "scripts/package_play.py", "scripts/flash_play.py",
)
PLAY_RUNTIME_PATHS = (
    "freeink-sdk", "platformio.ini", "src/main.cpp", "src/activities/Activity",
    "src/MappedInputManager", "lib/GfxRenderer/", "lib/hal/HalStorage",
)


class MaintenanceError(RuntimeError):
    """An update could not be prepared without guessing or overwriting work."""


def valid_tag(value: str) -> str:
    if not isinstance(value, str) or not TAG_RE.fullmatch(value):
        raise MaintenanceError("Unsupported release tag; expected a version such as v1.6.1")
    return value


def valid_sha(value: str) -> str:
    if not isinstance(value, str) or not SHA_RE.fullmatch(value):
        raise MaintenanceError("Expected a full, lowercase Git commit SHA")
    return value


def valid_path(value: str) -> str:
    if (not isinstance(value, str) or not PATH_RE.fullmatch(value)
            or value.startswith("/") or "//" in value or "/./" in value
            or any(part in (".", "..") or part.lower().rstrip(".") == ".git"
                   for part in PurePosixPath(value).parts)):
        raise MaintenanceError("Unsupported repository path: " + repr(value))
    return value


def validate_manifest(data: dict) -> dict:
    if data.get("schema_version") != 1:
        raise MaintenanceError("Unsupported upstream manifest schema")
    if data.get("repository") != "probablyangg/CrossInk-Play" or data.get("base_branch") != "main":
        raise MaintenanceError("This maintenance configuration targets probablyangg/CrossInk-Play main")
    for source, repository in UPSTREAMS.items():
        item = data.get(source, {})
        if item.get("repository") != repository:
            raise MaintenanceError("Unexpected upstream repository for " + source)
        valid_tag(item.get("tag"))
        valid_sha(item.get("commit"))
    mappings = data["crossplay"].get("mappings", [])
    if not isinstance(mappings, list) or not mappings:
        raise MaintenanceError("CrossPlay import mappings are required")
    for mapping in mappings:
        source, target = valid_path(mapping["source"]), valid_path(mapping["target"])
        if source.endswith("/") != target.endswith("/"):
            raise MaintenanceError("Import mapping must map a directory to a directory, or a file to a file")
        if not target.startswith(("src/apps_local/", "host-tests-play/", "assets_local/")):
            raise MaintenanceError("CrossPlay mapping escapes the imported game layer")
    for key in ("freeink_sdk", "simulator"):
        valid_sha(data.get("dependencies", {}).get(key))
    return data


def load_manifest(repo: Path) -> dict:
    return validate_manifest(json.loads((repo / MANIFEST_PATH).read_text()))


def git(repo: Path, *args: str, input_data: bytes | None = None,
        check: bool = True, extra_env: dict | None = None) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    env.update({"GIT_TERMINAL_PROMPT": "0", "GIT_MERGE_AUTOEDIT": "no"})
    if extra_env:
        env.update(extra_env)
    command = ["git", "-c", "core.hooksPath=" + os.devnull,
               "-c", "core.fsmonitor=false", "-c", "commit.gpgsign=false",
               "-c", "user.name=CrossInk Play maintenance",
               "-c", "user.email=41898282+github-actions[bot]@users.noreply.github.com", *args]
    result = subprocess.run(command, cwd=repo, env=env, input=input_data,
                            capture_output=True, timeout=180)
    if check and result.returncode:
        detail = result.stderr.decode("utf-8", "replace")[-3000:]
        raise MaintenanceError(f"Git {args[0]} failed: {detail}")
    return result


def git_text(repo: Path, *args: str) -> str:
    return git(repo, *args).stdout.decode("utf-8").strip()


class GitHub:
    def __init__(self, token: str | None = None):
        self.token = token or os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")

    def request(self, path: str, method: str = "GET", data=None, missing_ok: bool = False):
        if not path.startswith("/repos/") or "\n" in path or "\r" in path:
            raise MaintenanceError("Unsupported GitHub API path")
        headers = {"Accept": "application/vnd.github+json", "User-Agent": "CrossInk-Play-maintenance",
                   "X-GitHub-Api-Version": "2022-11-28"}
        if self.token:
            headers["Authorization"] = "Bearer " + self.token
        body = None if data is None else json.dumps(data).encode()
        if body is not None:
            headers["Content-Type"] = "application/json"
        request = Request("https://api.github.com" + path, data=body, headers=headers, method=method)
        try:
            with urlopen(request, timeout=30) as response:
                raw = response.read(MAX_API_BYTES + 1)
            if len(raw) > MAX_API_BYTES:
                raise MaintenanceError("GitHub response exceeded the maintenance size limit")
            return json.loads(raw) if raw else None
        except HTTPError as error:
            if missing_ok and error.code == 404:
                return None
            if method == "POST" and "/pulls" in path and error.code in (403, 422):
                raise MaintenanceError(
                    "GitHub could not create the draft pull request. Enable Settings > Actions > General > "
                    "Workflow permissions > Allow GitHub Actions to create and approve pull requests. "
                    "The prepared branch and local report are retained; no existing branch was overwritten."
                ) from error
            raise MaintenanceError(f"GitHub API returned HTTP {error.code} for {path}") from error
        except (URLError, TimeoutError) as error:
            raise MaintenanceError("GitHub could not be reached; retry the update check") from error

    def latest(self, repository: str) -> dict:
        release = self.request(f"/repos/{repository}/releases/latest")
        if release.get("draft") or release.get("prerelease"):
            raise MaintenanceError("The latest release endpoint returned a draft or prerelease")
        tag = valid_tag(release.get("tag_name"))
        obj = self.request(f"/repos/{repository}/git/ref/tags/{quote(tag, safe='')}")["object"]
        for _ in range(8):
            sha = valid_sha(obj.get("sha"))
            if obj.get("type") == "commit":
                return {"tag": tag, "commit": sha,
                        "url": f"https://github.com/{repository}/releases/tag/{quote(tag, safe='')}"}
            if obj.get("type") != "tag":
                break
            obj = self.request(f"/repos/{repository}/git/tags/{sha}")["object"]
        raise MaintenanceError("Release tag does not resolve to a commit")


def check_upstreams(manifest: dict, api: GitHub) -> dict:
    sources = {}
    for source, repository in UPSTREAMS.items():
        latest = api.latest(repository)
        installed = {key: manifest[source][key] for key in ("tag", "commit")}
        sources[source] = {"repository": repository, "installed": installed, "latest": latest,
                           "status": "current" if latest["commit"] == installed["commit"] else "update_available"}
    return {"checked_at": datetime.now(timezone.utc).isoformat(), "sources": sources}


def check_markdown(report: dict) -> str:
    lines = ["# CrossInk Play upstream check", "", "| Source | Imported | Latest stable | Status |",
             "| --- | --- | --- | --- |"]
    for source, item in report["sources"].items():
        lines.append(f"| {source} | {item['installed']['tag']} | [{item['latest']['tag']}]"
                     f"({item['latest']['url']}) | {item['status']} |")
    lines += ["", "Updates are prepared as draft pull requests. No firmware is installed or released by this check.", ""]
    return "\n".join(lines)


def write_output(directory: Path, name: str, report: dict, markdown: str):
    directory.mkdir(parents=True, exist_ok=True)
    (directory / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")
    (directory / f"{name}.md").write_text(markdown)


def changed_paths(repo: Path, before: str, after: str) -> list[str]:
    raw = git(repo, "diff", "--no-renames", "--name-only", "-z", before, after, "--").stdout
    # Names outside the imported paths are report data, not command syntax.
    # Preserve arbitrary Git names here; stricter checks apply before patching.
    return [path.decode("utf-8", "surrogateescape") for path in raw.split(b"\0") if path]


def mapped_path(path: str, mappings: list[dict]) -> str | None:
    matches = []
    for item in mappings:
        source, target = item["source"], item["target"]
        if path == source or (source.endswith("/") and path.startswith(source)):
            matches.append(target + path[len(source):])
    if len(matches) > 1:
        raise MaintenanceError("Overlapping import mappings for " + path)
    return valid_path(matches[0]) if matches else None


def game_titles(repo: Path, revision: str) -> set[str]:
    result = git(repo, "show", f"{revision}:src/apps_local/Shelf.cpp", check=False)
    if result.returncode:
        return set()
    text = result.stdout.decode("utf-8", "replace")
    block = re.search(r"\bkGames\s*\[\s*\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not block:
        raise MaintenanceError("The upstream game registry changed shape and needs manual review")
    return set(re.findall(r'\{\s*"([^"\n]{1,80})"\s*,', block.group(1)))


def version_only_ini_change(repo: Path, before: str, after: str) -> bool:
    """Allow only parsed release-version changes, not flags or dependency edits."""
    snapshots = []
    try:
        for revision in (before, after):
            result = git(repo, "show", f"{revision}:platformio.ini", check=False)
            if result.returncode:
                return False
            parser = configparser.ConfigParser(interpolation=None, strict=True)
            parser.read_string(result.stdout.decode("utf-8"))
            snapshots.append({"DEFAULT": dict(parser.defaults()),
                              **{section: dict(parser.items(section, raw=True)) for section in parser.sections()}})
        if snapshots[0].keys() != snapshots[1].keys():
            return False
        changed_version = False
        for section in ("crossplay", "crosspoint"):
            if section not in snapshots[0]:
                continue
            old = snapshots[0][section].get("version")
            new = snapshots[1][section].get("version")
            if old == new:
                continue
            valid_tag(old)
            valid_tag(new)
            changed_version = True
            del snapshots[0][section]["version"]
            del snapshots[1][section]["version"]
        return changed_version and snapshots[0] == snapshots[1]
    except (configparser.Error, UnicodeError, MaintenanceError):
        return False


def remapped_patch(repo: Path, before: str, after: str, source: str, target: str) -> bytes:
    patch = git(repo, "diff", "--binary", "--full-index", "--no-renames", before, after,
                "--", source).stdout
    if len(patch) > MAX_PATCH_BYTES:
        raise MaintenanceError("An imported patch exceeds the maintenance size limit")
    # Supported paths deliberately exclude whitespace/quotes. Only Git's path
    # headers are rewritten; matching text inside game code is never replaced.
    src, dst = source.encode(), target.encode()
    replacements = {
        b"diff --git a/" + src + b" b/" + src + b"\n": b"diff --git a/" + dst + b" b/" + dst + b"\n",
        b"--- a/" + src + b"\n": b"--- a/" + dst + b"\n",
        b"+++ b/" + src + b"\n": b"+++ b/" + dst + b"\n",
    }
    return b"".join(replacements.get(line, line) for line in patch.splitlines(keepends=True))


def report_markdown(report: dict) -> str:
    source = report["source"]
    existing_pr = report.get("publish_status") == "existing_pr_unchanged"
    display_status = "existing_pr_unchanged" if existing_pr else report["status"]
    lines = [f"# {source} update: {report['latest']['tag']}", "",
             f"Status: **{display_status}**", "",
             f"Imported baseline: `{report['installed']['tag']}` (`{report['installed']['commit']}`).",
             f"Candidate: [{report['latest']['tag']}]({report['latest']['url']}) "
             f"(`{report['latest']['commit']}`).", ""]
    if source in UPSTREAMS:
        lines += [f"[Upstream comparison](https://github.com/{UPSTREAMS[source]}/compare/"
                  f"{report['installed']['commit']}...{report['latest']['commit']})", ""]
    if existing_pr:
        lines += ["**The existing pull request and its branch were left unchanged.**",
                  "This run did not publish its newly prepared candidate. The candidate details below describe "
                  "local preparation only and must not be read as the contents or validation status of the linked PR.",
                  f"Local candidate preparation: `{report['status']}`."]
    elif report["status"] == "current":
        lines += ["The recorded source already matches the latest stable release. No branch or PR was created."]
    elif report["status"] == "needs_adaptation":
        lines += ["**Report only: this PR does not contain an applied firmware update.**",
                  "The imported baseline is unchanged. Resolve the items below before treating this as an update."]
    else:
        lines += ["The candidate contains code changes and a proposed source-baseline update.",
                  "It remains a draft for compatibility review; a clean Git operation is not a passed firmware test."]
    for key, heading in (("blockers", "Needs adaptation"), ("new_games", "New upstream games"),
                         ("imported_paths", "Imported paths"), ("unimported_paths", "Changes outside the game import")):
        values = report.get(key, [])
        if values:
            lines += ["", "## " + heading, ""]
            # Code spans keep upstream names as data, and the escaped @ avoids
            # creating mentions from a game title in an automated PR.
            lines += ["- `" + json.dumps(value, ensure_ascii=True).replace("@", "\\u0040").replace("`", "\\u0060")
                      + "`" for value in values[:150]]
            if len(values) > 150:
                lines += [f"- {len(values) - 150} additional paths are listed in the JSON report."]
    if report["status"] != "current":
        lines += ["", "## Before merging", "",
                  "- Review local Play adapters, reader integration, save formats, and dependency changes.",
                  "- Run Play - Build firmware on the actual PR branch; review the package and test output.",
                  "- Check the X4 Pro's reader/game/reader journey, saves, frontlight, and sleep/wake.",
                  "- Keep CrossInk merge ancestry when merging a CrossInk code-update PR.",
                  "", "This automation never merges PRs, publishes firmware releases, or flashes a device."]
    if report.get("pull_request"):
        lines += ["", "Pull request: " + report["pull_request"]]
    if report.get("publish_error"):
        lines += ["", "## Publishing did not complete", "", report["publish_error"]]
    return "\n".join(lines) + "\n"


def prepare_candidate(repo: Path, manifest: dict, source: str, latest: dict, output: Path) -> dict:
    """Create a candidate commit in the object database, not on the user's branch.

    Both source commits must already be fetched. This function is also the
    offline test seam: tests supply local Git histories and never use GitHub.
    """
    before, after = manifest[source]["commit"], valid_sha(latest["commit"])
    base = git_text(repo, "rev-parse", "HEAD")
    report = {"source": source, "installed": {key: manifest[source][key] for key in ("tag", "commit")},
              "latest": latest, "base_commit": base, "status": "current", "blockers": [],
              "new_games": [], "imported_paths": [], "unimported_paths": []}
    if before == after:
        write_output(output, "prepare", report, report_markdown(report))
        return report
    paths = changed_paths(repo, before, after)
    report["status"] = "prepared_code"
    if git(repo, "merge-base", "--is-ancestor", before, after, check=False).returncode:
        report["blockers"].append("The selected upstream release is not descended from the recorded baseline.")
    patches = []
    if source == "crossink":
        report["imported_paths"] = paths
        protected = [path for path in paths if any(path.startswith(prefix) for prefix in CONTROL_PATHS)]
        if protected:
            report["blockers"].append("Upstream changes maintenance/workflow control files: " + ", ".join(protected))
        if "freeink-sdk" in paths:
            report["blockers"].append("The pinned FreeInk SDK changed; its checked Play patch needs adaptation.")
    else:
        report["new_games"] = sorted(game_titles(repo, after) - game_titles(repo, before))
        if report["new_games"]:
            report["blockers"].append("New games need registration, dependency and save-format review before import.")
        for path in paths:
            try:
                target = mapped_path(path, manifest["crossplay"]["mappings"])
                if target:
                    valid_path(path)
            except MaintenanceError:
                report["blockers"].append("An imported path needs manual handling: " + repr(path))
                report["unimported_paths"].append(path)
                continue
            if target:
                report["imported_paths"].append(target)
                patches.append(remapped_patch(repo, before, after, path, target))
            else:
                report["unimported_paths"].append(path)
        runtime = [path for path in paths if any(path.startswith(prefix) for prefix in PLAY_RUNTIME_PATHS)
                   and not (path == "platformio.ini" and version_only_ini_change(repo, before, after))]
        if runtime:
            report["blockers"].append("Upstream runtime or dependency changes require compatibility review: "
                                      + ", ".join(runtime))
        new_helpers = [path for path in report["unimported_paths"]
                       if path.startswith("src/apps_local/") and len(PurePosixPath(path).parts) == 3]
        if new_helpers:
            report["blockers"].append("Unmapped shared game-layer files need review: " + ", ".join(new_helpers))
    output.mkdir(parents=True, exist_ok=True)
    if patches:
        joined = b"".join(patches)
        if len(joined) > MAX_PATCH_BYTES:
            raise MaintenanceError("Combined import patch exceeds the maintenance size limit")
        (output / "crossplay-import.patch").write_bytes(joined)

    with tempfile.TemporaryDirectory(prefix="crossink-play-update-") as temporary:
        worktree = Path(temporary) / "candidate"
        git(repo, "worktree", "add", "--detach", str(worktree), base)
        try:
            if not report["blockers"]:
                if source == "crossink":
                    applied = git(worktree, "merge", "--no-ff", "--no-commit", after, check=False)
                else:
                    applied = None
                    for patch in patches:
                        applied = git(worktree, "apply", "--3way", "--index", "--whitespace=nowarn",
                                      input_data=patch, check=False)
                        if applied.returncode:
                            break
                if applied is not None and applied.returncode:
                    conflicts = git_text(worktree, "diff", "--name-only", "--diff-filter=U")
                    report["blockers"].append("Git could not apply the update cleanly: "
                                              + (conflicts or "inspect the attached source comparison/patch"))
            if report["blockers"]:
                report["status"] = "needs_adaptation"
                # This is exclusively our temporary worktree; the caller's
                # checkout and any local SDK patch are untouched.
                git(worktree, "reset", "--hard", base)
            else:
                updated = copy.deepcopy(manifest)
                updated[source].update({"tag": latest["tag"], "commit": after})
                (worktree / MANIFEST_PATH).write_text(json.dumps(updated, indent=2) + "\n")
                git(worktree, "add", "--", MANIFEST_PATH)
            report_path = f"maintenance/updates/{source}-{latest['tag']}-{after[:8]}.md"
            (worktree / report_path).parent.mkdir(parents=True, exist_ok=True)
            (worktree / report_path).write_text(report_markdown(report))
            git(worktree, "add", "--", report_path)
            git(worktree, "commit", "-m", f"chore: prepare {source} update to {latest['tag']}")
            report["candidate_commit"] = git_text(worktree, "rev-parse", "HEAD")
            report["branch"] = f"chore/upstream-{source}-{latest['tag']}-{after[:8]}"
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree))
    write_output(output, "prepare", report, report_markdown(report))
    return report


def fetch_release(repo: Path, source: str, release: dict):
    tag, sha = valid_tag(release["tag"]), valid_sha(release["commit"])
    ref = f"refs/play-upstream/{source}/{sha}"
    url = f"https://github.com/{UPSTREAMS[source]}.git"
    git(repo, "fetch", "--no-tags", url, f"refs/tags/{tag}:{ref}")
    if git_text(repo, "rev-parse", ref + "^{commit}") != sha:
        raise MaintenanceError("The release tag changed during the check; retry after reviewing the upstream tag")


def publish_candidate(repo: Path, manifest: dict, report: dict, api: GitHub):
    if report["status"] == "current":
        return
    if not api.token:
        raise MaintenanceError("GITHUB_TOKEN is required to publish a draft update PR")
    repository, branch = manifest["repository"], report["branch"]
    owner = repository.split("/")[0]
    query = urlencode({"state": "all", "head": owner + ":" + branch, "per_page": 100})
    existing = api.request(f"/repos/{repository}/pulls?{query}")
    if existing:
        report["pull_request"] = existing[0]["html_url"]
        report["publish_status"] = "existing_pr_unchanged"
        report["candidate_published"] = False
        return
    remote_branch = api.request(f"/repos/{repository}/git/ref/heads/{quote(branch, safe='')}", missing_ok=True)
    if remote_branch:
        remote_sha = valid_sha(remote_branch["object"]["sha"])
        remote_commit = api.request(f"/repos/{repository}/git/commits/{remote_sha}")
        candidate_tree = git_text(repo, "rev-parse", report["candidate_commit"] + "^{tree}")
        candidate_parents = git_text(repo, "show", "-s", "--format=%P", report["candidate_commit"]).split()
        if (remote_commit["tree"]["sha"] != candidate_tree
                or [parent["sha"] for parent in remote_commit["parents"]] != candidate_parents):
            raise MaintenanceError("The update branch already exists with different changes; review it before retrying. "
                                   "No branch was overwritten: " + branch)
        # A prior run may have pushed successfully before PR creation was
        # denied. Reuse only the exact candidate tree and ancestry, never push
        # over an existing branch or silently adopt a human-edited candidate.
        report["candidate_commit"] = remote_sha
    else:
        credentials = base64.b64encode(("x-access-token:" + api.token).encode()).decode()
        # Empty the inherited multivalue header, then add this job's token only
        # to this push process. No argv, URL, config file, or report has it.
        push_env = {"GIT_CONFIG_COUNT": "2", "GIT_CONFIG_KEY_0": "http.https://github.com/.extraheader",
                    "GIT_CONFIG_VALUE_0": "", "GIT_CONFIG_KEY_1": "http.https://github.com/.extraheader",
                    "GIT_CONFIG_VALUE_1": "AUTHORIZATION: basic " + credentials}
        git(repo, "push", f"https://github.com/{repository}.git",
            report["candidate_commit"] + ":refs/heads/" + branch, extra_env=push_env)
    title = ("Adapt " if report["status"] == "needs_adaptation" else "Update ")
    title += report["source"] + " to " + report["latest"]["tag"]
    created = api.request(f"/repos/{repository}/pulls", method="POST", data={
        "title": title, "head": branch, "base": manifest["base_branch"],
        "draft": True, "body": report_markdown(report), "maintainer_can_modify": True,
    })
    report["pull_request"] = created["html_url"]
    report["publish_status"] = "draft_pr_created"
    report["candidate_published"] = True


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subcommands = parser.add_subparsers(dest="command", required=True)
    check_parser = subcommands.add_parser("check", help="Read latest stable releases; no repository changes")
    check_parser.add_argument("--output-dir", type=Path, default=ROOT / ".maintenance-output")
    prepare_parser = subcommands.add_parser("prepare", help="Prepare an isolated candidate; optionally publish a draft PR")
    prepare_parser.add_argument("--source", choices=tuple(UPSTREAMS), required=True)
    prepare_parser.add_argument("--output-dir", type=Path, default=ROOT / ".maintenance-output")
    prepare_parser.add_argument("--publish", action="store_true")
    args = parser.parse_args(argv)
    manifest, api = load_manifest(ROOT), GitHub()
    if args.command == "check":
        report = check_upstreams(manifest, api)
        write_output(args.output_dir, "check", report, check_markdown(report))
        print(check_markdown(report))
        return 0
    # Ignore the intentional dirty SDK files, but reject tracked source edits.
    dirty = git_text(ROOT, "status", "--porcelain", "--untracked-files=no", "--ignore-submodules=dirty")
    committed = json.loads(git_text(ROOT, "show", "HEAD:" + MANIFEST_PATH))
    if dirty or committed != manifest:
        raise MaintenanceError("Prepare updates from a committed checkout; local source changes were left untouched")
    latest = api.latest(UPSTREAMS[args.source])
    if latest["commit"] != manifest[args.source]["commit"]:
        fetch_release(ROOT, args.source, manifest[args.source])
        fetch_release(ROOT, args.source, latest)
    report = prepare_candidate(ROOT, manifest, args.source, latest, args.output_dir)
    if args.publish:
        try:
            publish_candidate(ROOT, manifest, report, api)
        except MaintenanceError as error:
            report["publish_error"] = str(error)
            write_output(args.output_dir, "prepare", report, report_markdown(report))
            raise
        write_output(args.output_dir, "prepare", report, report_markdown(report))
    print(report_markdown(report))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (MaintenanceError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
        print("Maintenance error: " + str(error), file=sys.stderr)
        raise SystemExit(1)
