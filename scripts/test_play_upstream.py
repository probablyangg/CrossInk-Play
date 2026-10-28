#!/usr/bin/env python3
"""Offline checks of update semantics using disposable Git histories."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import play_upstream as maintenance


class RepoFixture:
    def __init__(self, root: Path):
        self.root = root
        self.repo = root / "repo"
        self.repo.mkdir()
        self.run("init", "-b", "main")
        self.write("reader.txt", "font setting\nparagraph setting\n")
        self.write("src/apps_local/chess/rules.cpp", "opening\nunchanged\nendgame\n")
        self.write("host-tests/chess/test.cpp", "original test\n")
        self.write("LICENSE", "original license\n")
        self.write("platformio.ini", "[crosspoint]\nversion = 1.6.5\n[crossplay]\nversion = 1.14.2\n"
                   "[base]\nbuild_flags = -DORIGINAL\n")
        self.write("src/apps_local/Shelf.cpp", 'constexpr shelf::Item kGames[] = {\n    {"CHESS", icon, factory},\n};\n')
        self.baseline = self.commit("upstream baseline")
        self.manifest = {
            "schema_version": 1, "repository": "probablyangg/CrossInk-Play", "base_branch": "main",
            "crossink": {"repository": "uxjulia/CrossInk", "tag": "v1.6.1", "commit": self.baseline},
            "crossplay": {"repository": "ma-r-s/crossplay", "tag": "v1.14.2", "commit": self.baseline,
                          "mappings": [
                              {"source": "src/apps_local/chess/", "target": "src/apps_local/chess/"},
                              {"source": "host-tests/chess/", "target": "host-tests-play/chess/"},
                              {"source": "LICENSE", "target": "src/apps_local/LICENSE.CrossPlay"},
                          ]},
            "dependencies": {"freeink_sdk": "1" * 40, "simulator": "2" * 40},
        }
        self.write(maintenance.MANIFEST_PATH, json.dumps(self.manifest, indent=2) + "\n")
        self.run("mv", "host-tests/chess", "host-tests-play-chess")
        (self.repo / "host-tests-play").mkdir()
        self.run("mv", "host-tests-play-chess", "host-tests-play/chess")
        self.write("src/apps_local/LICENSE.CrossPlay", "original license\n")
        self.write("play-only.txt", "keep this\n")
        self.play_head = self.commit("local Play adaptations")

    def run(self, *args):
        return maintenance.git_text(self.repo, *args)

    def write(self, path, data):
        destination = self.repo / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(data)

    def commit(self, message):
        self.run("add", "--all")
        self.run("commit", "-m", message)
        return self.run("rev-parse", "HEAD")

    def upstream(self, changes):
        self.run("checkout", "--detach", self.baseline)
        for path, value in changes.items():
            if value is None:
                (self.repo / path).unlink()
            else:
                self.write(path, value)
        latest = self.commit("upstream change")
        self.run("checkout", "main")
        return latest

    def prepare(self, source, commit):
        return maintenance.prepare_candidate(
            self.repo, copy.deepcopy(self.manifest), source,
            {"tag": "v2.0.0", "commit": commit, "url": "https://github.com/example/repo/releases/tag/v2.0.0"},
            self.root / "output",
        )

    def show(self, report, path):
        return self.run("show", report["candidate_commit"] + ":" + path)


class CandidateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="play-maintenance-test-")
        self.fixture = RepoFixture(Path(self.temporary.name))

    def tearDown(self):
        self.temporary.cleanup()

    def test_clean_crossink_merge_preserves_ancestry_and_play_changes(self):
        f = self.fixture
        latest = f.upstream({"reader.txt": "improved font setting\nparagraph setting\n"})
        report = f.prepare("crossink", latest)
        self.assertEqual(report["status"], "prepared_code")
        self.assertEqual(f.show(report, "play-only.txt"), "keep this")
        self.assertIn("improved font", f.show(report, "reader.txt"))
        self.assertEqual(f.run("show", "-s", "--format=%P", report["candidate_commit"]).split(),
                         [f.play_head, latest])
        self.assertEqual(json.loads(f.show(report, maintenance.MANIFEST_PATH))["crossink"]["commit"], latest)
        self.assertEqual(f.run("rev-parse", "HEAD"), f.play_head)
        self.assertEqual(f.run("status", "--porcelain"), "")

    def test_conflict_produces_report_only_without_advancing_baseline(self):
        f = self.fixture
        f.write("reader.txt", "local font setting\nparagraph setting\n")
        f.play_head = f.commit("local reader adaptation")
        latest = f.upstream({"reader.txt": "upstream font setting\nparagraph setting\n"})
        report = f.prepare("crossink", latest)
        self.assertEqual(report["status"], "needs_adaptation")
        self.assertEqual(f.show(report, "reader.txt"), "local font setting\nparagraph setting")
        self.assertEqual(json.loads(f.show(report, maintenance.MANIFEST_PATH))["crossink"]["commit"], f.baseline)
        self.assertEqual(f.run("show", "-s", "--format=%P", report["candidate_commit"]), f.play_head)
        self.assertIn("Report only", (f.root / "output/prepare.md").read_text())

    def test_crossplay_three_way_preserves_local_adaptation_and_maps_tests(self):
        f = self.fixture
        # Separate distant lines give the three-way merge a genuine local change.
        original = "\n".join(f"line {index}" for index in range(30)) + "\n"
        f.run("checkout", "--detach", f.baseline)
        f.write("src/apps_local/chess/rules.cpp", original)
        f.baseline = f.commit("expanded game fixture")
        f.manifest["crossplay"]["commit"] = f.baseline
        f.run("checkout", "main")
        f.write("src/apps_local/chess/rules.cpp", original.replace("line 1\n", "local input adapter\n"))
        f.write(maintenance.MANIFEST_PATH, json.dumps(f.manifest, indent=2) + "\n")
        f.play_head = f.commit("local input adaptation")
        latest = f.upstream({"src/apps_local/chess/rules.cpp": original.replace("line 25\n", "new game rule\n"),
                             "host-tests/chess/test.cpp": "improved test\n",
                             "LICENSE": "updated license\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["status"], "prepared_code", report)
        self.assertIn("local input adapter", f.show(report, "src/apps_local/chess/rules.cpp"))
        self.assertIn("new game rule", f.show(report, "src/apps_local/chess/rules.cpp"))
        self.assertEqual(f.show(report, "host-tests-play/chess/test.cpp"), "improved test")
        self.assertEqual(f.show(report, "src/apps_local/LICENSE.CrossPlay"), "updated license")
        self.assertEqual(json.loads(f.show(report, maintenance.MANIFEST_PATH))["crossplay"]["commit"], latest)

    def test_crossplay_conflict_rolls_back_all_imported_files(self):
        f = self.fixture
        f.write("src/apps_local/chess/rules.cpp", "local different rules\n")
        f.play_head = f.commit("local rules")
        latest = f.upstream({"src/apps_local/chess/rules.cpp": "upstream different rules\n",
                             "host-tests/chess/test.cpp": "new test\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["status"], "needs_adaptation")
        self.assertEqual(f.show(report, "host-tests-play/chess/test.cpp"), "original test")
        self.assertEqual(f.show(report, "src/apps_local/chess/rules.cpp"), "local different rules")
        self.assertEqual(json.loads(f.show(report, maintenance.MANIFEST_PATH))["crossplay"]["commit"], f.baseline)

    def test_new_game_is_reported_without_installing_or_claiming_it(self):
        f = self.fixture
        latest = f.upstream({"src/apps_local/Shelf.cpp":
                             'constexpr shelf::Item kGames[] = {\n    {"CHESS", icon, factory},\n'
                             '    {"REVERSI", icon, factory},\n};\n',
                             "src/apps_local/reversi/Reversi.cpp": "new game\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["new_games"], ["REVERSI"])
        self.assertEqual(report["status"], "needs_adaptation")
        self.assertNotIn("REVERSI", f.show(report, "src/apps_local/Shelf.cpp"))

    def test_upstream_workflow_changes_are_report_only(self):
        f = self.fixture
        latest = f.upstream({".github/workflows/ci.yml": "unreviewed workflow\n"})
        report = f.prepare("crossink", latest)
        self.assertEqual(report["status"], "needs_adaptation")
        self.assertIn("control files", report["blockers"][0])

    def test_runtime_change_requires_adaptation(self):
        f = self.fixture
        latest = f.upstream({"src/activities/Activity.cpp": "new runtime API\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["status"], "needs_adaptation")
        self.assertEqual(json.loads(f.show(report, maintenance.MANIFEST_PATH))["crossplay"]["commit"], f.baseline)

    def test_game_fix_with_release_version_bump_can_be_prepared(self):
        f = self.fixture
        latest = f.upstream({"src/apps_local/chess/rules.cpp": "fixed game rules\n",
                             "platformio.ini": "[crosspoint]\nversion = 1.6.6\n[crossplay]\nversion = 1.14.3\n"
                             "[base]\nbuild_flags = -DORIGINAL\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["status"], "prepared_code", report)
        self.assertEqual(f.show(report, "src/apps_local/chess/rules.cpp"), "fixed game rules")
        self.assertIn("version = 1.14.2", f.show(report, "platformio.ini"))

    def test_version_bump_with_build_flag_change_still_requires_adaptation(self):
        f = self.fixture
        latest = f.upstream({"platformio.ini": "[crosspoint]\nversion = 1.6.5\n[crossplay]\nversion = 1.14.3\n"
                             "[base]\nbuild_flags = -DCHANGED\n"})
        self.assertEqual(f.prepare("crossplay", latest)["status"], "needs_adaptation")

    def test_unrelated_document_names_can_be_reported(self):
        f = self.fixture
        latest = f.upstream({"docs/new guide é.md": "unrelated documentation\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["status"], "prepared_code")
        self.assertIn("docs/new guide é.md", report["unimported_paths"])

    def test_unsupported_imported_filename_becomes_adaptation_report(self):
        f = self.fixture
        latest = f.upstream({"src/apps_local/chess/new rules.cpp": "needs manual import\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["status"], "needs_adaptation")
        self.assertIn("manual handling", report["blockers"][0])

    def test_noop_creates_no_candidate(self):
        f = self.fixture
        report = f.prepare("crossink", f.baseline)
        self.assertEqual(report["status"], "current")
        self.assertNotIn("candidate_commit", report)
        self.assertEqual(f.run("rev-parse", "HEAD"), f.play_head)
        self.assertEqual(f.run("worktree", "list", "--porcelain").count("worktree "), 1)

    def test_new_file_and_delete_are_mapped(self):
        f = self.fixture
        latest = f.upstream({"host-tests/chess/test.cpp": None, "host-tests/chess/extra.cpp": "new test\n"})
        report = f.prepare("crossplay", latest)
        self.assertEqual(report["status"], "prepared_code")
        self.assertEqual(f.show(report, "host-tests-play/chess/extra.cpp"), "new test")
        result = maintenance.git(f.repo, "cat-file", "-e",
                                 report["candidate_commit"] + ":host-tests-play/chess/test.cpp", check=False)
        self.assertNotEqual(result.returncode, 0)


class InputAndPublishTests(unittest.TestCase):
    def test_rejects_shell_and_ref_syntax(self):
        for value in ("--upload-pack=sh", "v1.2.3;touch-x", "v1.2.3\nmain", "$(id)", "../main", "v1.2.3^{tree}"):
            with self.subTest(value=value), self.assertRaises(maintenance.MaintenanceError):
                maintenance.valid_tag(value)

    def test_rejects_escaping_path_and_accepts_expected_paths(self):
        for value in ("../reader.cpp", "/tmp/file", "src/../.git/config", "src/a b.cpp", ".git/config"):
            with self.subTest(value=value), self.assertRaises(maintenance.MaintenanceError):
                maintenance.valid_path(value)
        self.assertEqual(maintenance.valid_path("src/apps_local/chess/"), "src/apps_local/chess/")

    def test_existing_pr_is_not_overwritten_or_reopened(self):
        class FakeApi:
            token = "fixture-token"

            def request(self, path, **kwargs):
                self.path = path
                return [{"html_url": "https://github.com/probablyangg/CrossInk-Play/pull/10", "state": "closed"}]

        report = {"status": "prepared_code", "branch": "chore/upstream-crossink-v2.0.0-12345678",
                  "source": "crossink", "installed": {"tag": "v1.0.0", "commit": "a" * 40},
                  "latest": {"tag": "v2.0.0", "commit": "b" * 40, "url": "https://github.com/example/repo"}}
        api = FakeApi()
        with patch.object(maintenance, "git", side_effect=AssertionError("must not push")):
            maintenance.publish_candidate(Path("."), {"repository": "probablyangg/CrossInk-Play"}, report, api)
        self.assertEqual(report["publish_status"], "existing_pr_unchanged")
        self.assertTrue(report["pull_request"].endswith("/10"))
        self.assertFalse(report["candidate_published"])
        rendered = maintenance.report_markdown(report)
        self.assertIn("**existing_pr_unchanged**", rendered)
        self.assertIn("did not publish its newly prepared candidate", rendered)
        self.assertNotIn("The candidate contains code changes", rendered)

    def test_check_resolves_current_and_available_independently(self):
        class FakeApi:
            def latest(self, repository):
                return {"tag": "v1.2.3", "commit": "a" * 40 if repository == "uxjulia/CrossInk" else "c" * 40,
                        "url": "https://github.com/" + repository}

        report = maintenance.check_upstreams(
            {"crossink": {"tag": "v1.2.3", "commit": "a" * 40},
             "crossplay": {"tag": "v1.2.2", "commit": "b" * 40}}, FakeApi())
        self.assertEqual(report["sources"]["crossink"]["status"], "current")
        self.assertEqual(report["sources"]["crossplay"]["status"], "update_available")

    def test_report_cannot_create_mentions_from_upstream_game_names(self):
        report = {"source": "crossplay", "status": "needs_adaptation",
                  "installed": {"tag": "v1.0.0", "commit": "a" * 40},
                  "latest": {"tag": "v2.0.0", "commit": "b" * 40, "url": "https://github.com/example/repo"},
                  "new_games": ["@person `link`"]}
        rendered = maintenance.report_markdown(report)
        self.assertNotIn("@person", rendered)
        self.assertIn("\\u0040person", rendered)

    def test_human_edited_existing_branch_is_not_overwritten(self):
        class FakeApi:
            token = "fixture-token"

            def request(self, path, **kwargs):
                if "/pulls?" in path:
                    return []
                if "/git/ref/" in path:
                    return {"object": {"sha": "c" * 40}}
                return {"tree": {"sha": "d" * 40}, "parents": [{"sha": "a" * 40}]}

        report = {"status": "prepared_code", "branch": "chore/upstream-crossink-v2.0.0-12345678",
                  "candidate_commit": "b" * 40}
        with patch.object(maintenance, "git_text", side_effect=["e" * 40, "a" * 40]), \
                patch.object(maintenance, "git", side_effect=AssertionError("must not push")), \
                self.assertRaisesRegex(maintenance.MaintenanceError, "No branch was overwritten"):
            maintenance.publish_candidate(Path("."), {"repository": "probablyangg/CrossInk-Play"}, report, FakeApi())


if __name__ == "__main__":
    unittest.main()
