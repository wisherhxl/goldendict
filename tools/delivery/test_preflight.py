"""Executable publication safety tests; every receipt/remote is a fixture."""
import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import preflight as gate


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="goldendict-gate-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo"
        self.remote = self.root / "origin.git"
        self.run_git(self.root, "init", "--bare", str(self.remote))
        self.run_git(self.root, "init", str(self.repo))
        for key, value in (("user.name", "Fixture"), ("user.email", "fixture@example.invalid"), ("core.autocrlf", "false")):
            self.run_git(self.repo, "config", key, value)
        self.run_git(self.repo, "checkout", "-b", "feature/tiger-qt6-migration")
        (self.repo / "baseline.txt").write_text("preserve baseline\n", newline="\n")
        (self.repo / "docs").mkdir()
        (self.repo / "docs/agent-workflow.md").write_text("fixture contract\n", newline="\n")
        self.commit("baseline")
        self.base = self.run_git(self.repo, "rev-parse", "HEAD")
        self.run_git(self.repo, "remote", "add", "origin", str(self.remote))
        self.run_git(self.repo, "push", "origin", "HEAD")
        self.run_git(self.repo, "checkout", "-b", "chore/fixture")
        self.canonical = self.root / "canonical"
        self.run_git(self.repo, "worktree", "add", str(self.canonical), "feature/tiger-qt6-migration")
        (self.repo / "task.txt").write_text("task\n", newline="\n")
        self.commit("candidate")
        self.candidate = self.run_git(self.repo, "rev-parse", "HEAD")
        self.tree = self.run_git(self.repo, "rev-parse", "HEAD^{tree}")
        self.a = dict(profile=gate.PROFILE, target=gate.TARGET, operations=["push-task", "publish-baseline"],
                      approval_source="TEST FIXTURE ONLY", developer_id="fixture-developer", remote="origin",
                      update="fast-forward", base=self.base, candidate=self.candidate, tree=self.tree,
                      task_ref="refs/heads/chore/fixture", remote_url=str(self.remote))
        self.ctx = dict(profile=gate.PROFILE, scope="fixture", environment_observation="fixture observation",
                        evidence=[])
        for kind in ("requirements", "policy", "verification", "environment"):
            path = self.root / (kind + ".txt")
            path.write_text("fixture " + kind)
            self.ctx["evidence"].append(dict(kind=kind, path=str(path), sha256=gate.digest(path)))
        self.r = dict(result="Pass", required_findings=[], reviewer_id="fixture-reviewer",
                      origin_reference="synthetic-test-not-a-real-audit", fresh_no_history=True,
                      fixture=True, base=self.base, candidate=self.candidate, tree=self.tree, scope="fixture")
        self.activation = self.root / ".ai-work-model-activation.json"
        installed = []
        for role, path in {"global-entry": self.root / "fixture-codex/AGENTS.md",
                           "global-delivery": self.root / "fixture-codex/policies/engineering-delivery.md",
                           "workspace-entry": self.root / "AGENTS.md",
                           "project-workflow": self.canonical / "docs/agent-workflow.md"}.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists(): path.write_text("fixture policy")
            installed.append(dict(role=role, path=str(path), sha256=gate.digest(path)))
        self.activation.write_text(json.dumps(dict(enabled=True, profile=gate.PROFILE, files=installed)))
        self.a["activation_path"] = str(self.activation)
        self.save()

    def run_git(self, cwd, *args):
        p = subprocess.run(["git", "-C", str(cwd), *args], capture_output=True, text=True, check=True)
        return p.stdout.strip()

    def commit(self, message):
        self.run_git(self.repo, "add", ".")
        self.run_git(self.repo, "commit", "-m", message)

    def save(self):
        self.ap, self.rp, self.cp = [self.root / (n + ".json") for n in ("authority", "receipt", "context")]
        self.ap.write_text(json.dumps(self.a))
        self.cp.write_text(json.dumps(self.ctx))
        self.r.update(authority_sha256=gate.digest(self.ap), context_sha256=gate.digest(self.cp),
                      activation_sha256=gate.digest(self.activation))
        self.rp.write_text(json.dumps(self.r))

    def check(self, operation="push-task", fixture=True):
        return gate.check(self.repo, self.ap, self.rp, gate.digest(self.rp), operation, self.cp, fixture=fixture)

    def test_valid_task_and_baseline_publication_read_only(self):
        before = self.run_git(self.repo, "show-ref")
        self.assertEqual(self.check()["result"], "Eligible")
        self.run_git(self.repo, "push", "origin", "HEAD")
        self.assertEqual(self.check("publish-baseline")["result"], "Eligible")
        self.assertEqual(before.splitlines()[0], self.run_git(self.repo, "show-ref").splitlines()[0])
        self.assertEqual(self.run_git(self.repo, "status", "--porcelain"), "")

    def test_candidate_changed(self):
        (self.repo / "task.txt").write_text("changed\n")
        self.commit("changed")
        with self.assertRaises(gate.Rejected): self.check()

    def test_baseline_advanced(self):
        self.run_git(self.repo, "push", "origin", "HEAD:" + gate.TARGET)
        with self.assertRaises(gate.Rejected): self.check()

    def test_wrong_target(self):
        self.a["target"] = "refs/heads/master"
        self.save()
        with self.assertRaises(gate.Rejected): self.check()

    def test_missing_authority(self):
        self.a["operations"] = []
        self.save()
        with self.assertRaises(gate.Rejected): self.check()

    def test_missing_receipt(self):
        self.rp.unlink()
        with self.assertRaises(OSError): self.check()

    def test_failed_review(self):
        self.r["result"] = "Fail"
        self.save()
        with self.assertRaises(gate.Rejected): self.check()

    def test_self_review(self):
        self.r["reviewer_id"] = self.a["developer_id"]
        self.save()
        with self.assertRaises(gate.Rejected): self.check()

    def test_stale_evidence(self):
        Path(self.ctx["evidence"][0]["path"]).write_text("requirement changed")
        with self.assertRaises(gate.Rejected): self.check()

    def test_missing_evidence(self):
        Path(self.ctx["evidence"][0]["path"]).unlink()
        with self.assertRaises(gate.Rejected): self.check()

    def test_dirty_checkout(self):
        (self.repo / "untracked.txt").write_text("preserve")
        with self.assertRaises(gate.Rejected): self.check()

    def test_unfinished_operation(self):
        (self.repo / ".git" / "CHERRY_PICK_HEAD").write_text(self.base)
        with self.assertRaises(gate.Rejected): self.check()

    def test_wrong_checkout_role(self):
        self.run_git(self.repo, "checkout", "-b", "chore/other")
        with self.assertRaises(gate.Rejected): self.check()

    def test_task_not_pushed(self):
        with self.assertRaises(gate.Rejected): self.check("publish-baseline")

    def test_fixture_not_real_review(self):
        with self.assertRaises(gate.Rejected): self.check(fixture=False)

    def test_receipt_digest_mismatch(self):
        with self.assertRaises(gate.Rejected):
            gate.check(self.repo, self.ap, self.rp, "0" * 64, "push-task", self.cp, fixture=True)

    def test_discussion_authority_never_publishes(self):
        self.a["operations"] = ["discuss"]
        self.save()
        before = self.run_git(self.repo, "status", "--porcelain")
        with self.assertRaises(gate.Rejected): self.check()
        self.assertEqual(before, self.run_git(self.repo, "status", "--porcelain"))

    def test_pending_activation_blocks(self):
        obj = json.loads(self.activation.read_text())
        obj["enabled"] = False
        self.activation.write_text(json.dumps(obj))
        with self.assertRaises(gate.Rejected): self.check()

    def test_partial_install_blocks(self):
        (self.root / "fixture-codex/AGENTS.md").write_text("unreviewed policy install")
        with self.assertRaises(gate.Rejected): self.check()

    def test_correct_base_integration_preserves_new_content(self):
        # Real fixture integration, not reparenting an old task tree.
        self.run_git(self.repo, "checkout", "-b", "fixture-new-base", self.base)
        (self.repo / "new-baseline.txt").write_text("must survive\n", newline="\n")
        self.commit("new base")
        newbase = self.run_git(self.repo, "rev-parse", "HEAD")
        self.run_git(self.repo, "checkout", "-b", "chore/integrated")
        self.run_git(self.repo, "cherry-pick", self.candidate)
        self.assertEqual((self.repo / "new-baseline.txt").read_text(), "must survive\n")
        self.assertEqual(self.run_git(self.repo, "diff", "--name-only", newbase, "HEAD"), "task.txt")
        with self.assertRaises(gate.Rejected): self.check()

    def test_activation_replaced_after_review(self):
        obj = json.loads(self.activation.read_text())
        obj["files"] = [obj["files"][0]] * 4
        self.activation.write_text(json.dumps(obj))
        with self.assertRaises(gate.Rejected): self.check()

    def test_activation_duplicate_roles_even_if_receipt_bound(self):
        obj = json.loads(self.activation.read_text())
        obj["files"] = [obj["files"][0]] * 4
        self.activation.write_text(json.dumps(obj))
        self.save()
        with self.assertRaises(gate.Rejected): self.check()

    def test_activation_missing_role(self):
        obj = json.loads(self.activation.read_text())
        obj["files"].pop()
        self.activation.write_text(json.dumps(obj))
        self.save()
        with self.assertRaises(gate.Rejected): self.check()

    def test_additional_push_url(self):
        self.run_git(self.repo, "config", "--add", "remote.origin.pushurl", str(self.remote))
        self.run_git(self.repo, "config", "--add", "remote.origin.pushurl", str(self.root / "unauthorized.git"))
        with self.assertRaises(gate.Rejected): self.check()

    def test_follow_tags_rejected_and_explicit_push_does_not_publish_tag(self):
        self.run_git(self.repo, "config", "push.followTags", "true")
        self.run_git(self.repo, "tag", "-a", "unauthorized-release", "-m", "fixture only")
        with self.assertRaises(gate.Rejected): self.check()
        self.run_git(self.repo, "push", "--no-follow-tags", "--recurse-submodules=no", "origin",
                     self.candidate + ":refs/heads/chore/fixture")
        self.assertEqual(self.run_git(self.repo, "ls-remote", "--tags", "origin"), "")

    def test_remote_mirror_rejected(self):
        self.run_git(self.repo, "config", "remote.origin.mirror", "true")
        with self.assertRaises(gate.Rejected): self.check()

    def test_recursive_submodule_push_rejected(self):
        self.run_git(self.repo, "config", "push.recurseSubmodules", "on-demand")
        with self.assertRaises(gate.Rejected): self.check()

    def test_pending_canonical_operation(self):
        path = Path(self.run_git(self.canonical, "rev-parse", "--git-path", "CHERRY_PICK_HEAD"))
        path.write_text(self.base)
        with self.assertRaises(gate.Rejected): self.check()

    def test_pre_push_hook_rejected(self):
        (self.repo / ".git/hooks/pre-push").write_text("#!/bin/sh\nexit 0\n")
        with self.assertRaises(gate.Rejected): self.check()

    def test_wrong_installed_policy_path(self):
        obj = json.loads(self.activation.read_text())
        obj["files"][0]["path"] = obj["files"][1]["path"]
        obj["files"][0]["sha256"] = obj["files"][1]["sha256"]
        self.activation.write_text(json.dumps(obj))
        self.save()
        with self.assertRaises(gate.Rejected): self.check()


if __name__ == "__main__":
    unittest.main()
