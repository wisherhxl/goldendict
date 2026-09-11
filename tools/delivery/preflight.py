#!/usr/bin/env python3
"""Read-only GoldenDict publication checks. Never commits, fetches or pushes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

TARGET = "refs/heads/feature/tiger-qt6-migration"
PROFILE = "goldendict-candidate-v1"


class Rejected(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Rejected(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def git(repo, *args):
    env = dict(os.environ, GIT_OPTIONAL_LOCKS="0", GIT_TERMINAL_PROMPT="0")
    result = subprocess.run(["git", "-C", str(repo), *args], capture_output=True,
                            text=True, encoding="utf-8", env=env, timeout=45)
    require(result.returncode == 0, "git inspection failed: " + result.stderr.strip())
    return result.stdout.strip()


def check(repo, authority_path, receipt_path, receipt_hash, operation,
          context_path, *, fixture=False):
    repo = Path(repo).resolve()
    a, r, ctx = map(read_json, (authority_path, receipt_path, context_path))
    require(digest(receipt_path) == receipt_hash, "review receipt digest changed")
    require(a["profile"] == PROFILE and a["target"] == TARGET, "unauthorized profile/target")
    common = Path(git(repo, "rev-parse", "--path-format=absolute", "--git-common-dir"))
    workspace = common.parent.parent
    require(Path(a["activation_path"]).resolve() == workspace / ".ai-work-model-activation.json",
            "wrong workspace activation path")
    require(r["activation_sha256"] == digest(a["activation_path"]), "activation changed after review")
    activation = read_json(a["activation_path"])
    require(activation["enabled"] is True and activation["profile"] == PROFILE,
            "profile not activated")
    canonical = None
    for record in git(repo, "worktree", "list", "--porcelain").split("\n\n"):
        lines = record.splitlines()
        if "branch " + TARGET in lines:
            canonical = Path(lines[0].removeprefix("worktree ")).resolve()
    require(canonical is not None, "canonical checkout not registered")
    user_root = workspace / "fixture-codex" if fixture else Path(os.environ.get("CODEX_HOME", Path.home() / ".codex")).resolve()
    expected = {"global-entry": user_root / "AGENTS.md",
                "global-delivery": user_root / "policies/engineering-delivery.md",
                "workspace-entry": workspace / "AGENTS.md",
                "project-workflow": canonical / "docs/agent-workflow.md"}
    require(len(activation["files"]) == len(expected) and
            {x["role"] for x in activation["files"]} == set(expected), "incomplete/duplicate activation roles")
    for item in activation["files"]:
        require(Path(item["path"]).resolve() == expected[item["role"]].resolve(), "wrong installed policy path")
        require(Path(item["path"]).is_file() and digest(item["path"]) == item["sha256"],
                "partial/stale policy installation")
    require(git(canonical, "status", "--porcelain", "--untracked-files=all") == "", "canonical checkout not clean")
    require(operation in ("push-task", "publish-baseline") and operation in a["operations"],
            "operation not authorized")
    require(a["approval_source"].strip() and a["developer_id"].strip(), "missing authorization source")
    require(a["remote"] == "origin" and a["update"] == "fast-forward", "update not authorized")
    require(r["result"] == "Pass" and not r["required_findings"], "review did not pass")
    require(r["reviewer_id"].strip() and r["reviewer_id"] != a["developer_id"], "review is not independent")
    require(r["origin_reference"].strip() and r["fresh_no_history"] is True, "missing review provenance")
    require(r.get("fixture", False) is False or fixture, "test receipt cannot authorize publication")
    require(r["authority_sha256"] == digest(authority_path), "authority differs from review")
    require(r["context_sha256"] == digest(context_path), "evidence applicability changed")
    require(ctx["profile"] == PROFILE and ctx["scope"] == r["scope"], "scope/profile changed")
    require(ctx["environment_observation"].strip(), "missing environment observation")
    require({e["kind"] for e in ctx["evidence"]} >= {"requirements", "policy", "verification", "environment"},
            "missing required evidence category")
    for item in ctx["evidence"]:
        path = Path(item["path"])
        if not path.is_absolute():
            path = Path(context_path).resolve().parent / path
        require(path.is_file() and digest(path) == item["sha256"], "missing/stale evidence: " + str(path))
    b, c, t = r["base"], r["candidate"], r["tree"]
    for oid in (b, c, t):
        require(len(oid) == 40 and all(ch in "0123456789abcdef" for ch in oid), "not an exact object ID")
    require(a["base"] == b and a["candidate"] == c and a["tree"] == t, "candidate authority mismatch")
    require(git(repo, "rev-parse", "HEAD") == c, "checkout does not contain reviewed candidate")
    require(git(repo, "rev-parse", c + "^{tree}") == t, "candidate tree changed")
    require(git(repo, "rev-list", "--parents", "-n", "1", c).split() == [c, b],
            "candidate is not the reviewed base's single-parent direct successor")
    require(git(repo, "status", "--porcelain", "--untracked-files=all") == "", "checkout is not clean")
    require(git(repo, "diff", "--check", b, c) == "", "invalid whitespace")
    for checkout in (repo, canonical):
        for state in ("MERGE_HEAD", "CHERRY_PICK_HEAD", "REVERT_HEAD", "rebase-merge", "rebase-apply", "sequencer", "BISECT_START", "index.lock"):
            p = Path(git(checkout, "rev-parse", "--git-path", state))
            if not p.is_absolute():
                p = checkout / p
            require(not p.exists(), "unfinished Git operation: " + state)
    branch = git(repo, "symbolic-ref", "--quiet", "HEAD")
    require(branch == a["task_ref"] and branch not in (TARGET, "refs/heads/master", "refs/heads/main"),
            "wrong checkout role")
    require(branch.startswith("refs/heads/") and branch.count("/") >= 3, "invalid task branch")
    require(branch.split("/")[2] in ("feature", "fix", "docs", "test", "opt", "chore"), "protected/non-task branch")
    for key in ("push.followTags", "remote.origin.mirror"):
        require(git(repo, "config", "--type=bool", "--default=false", "--get", key) == "false",
                "extra publication behavior: " + key)
    require(git(repo, "config", "--default=no", "--get", "push.recurseSubmodules") in ("no", "false", "check"),
            "automatic submodule publication enabled")
    hook = Path(git(repo, "rev-parse", "--git-path", "hooks/pre-push"))
    if not hook.is_absolute():
        hook = repo / hook
    require(not hook.exists(), "pre-push hook requires separate side-effect review")
    require(git(repo, "remote", "get-url", "--all", "origin").splitlines() == [a["remote_url"]], "remote URLs changed")
    require(git(repo, "remote", "get-url", "--push", "--all", "origin").splitlines() == [a["remote_url"]], "push URLs differ")
    target_tip = git(repo, "ls-remote", "--refs", "origin", TARGET).split()
    require(target_tip == [b, TARGET], "remote baseline changed/missing")
    local_tip = git(repo, "rev-parse", TARGET)
    require(local_tip == b, "local baseline changed")
    remote_task = git(repo, "ls-remote", "--refs", "origin", branch).split()
    if operation == "publish-baseline":
        require(remote_task == [c, branch], "task branch has not delivered the exact candidate")
    elif remote_task:
        require(remote_task == [c, branch], "existing remote task identity differs; reconcile explicitly")
    return {"result": "Eligible", "operation": operation, "base": b, "candidate": c,
            "tree": t, "target": TARGET, "review_sha256": receipt_hash,
            "limitations": "Mechanical eligibility only; not receipt authentication or a remote transaction lock."}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "authority", "receipt", "review-sha256", "context", "operation"):
        p.add_argument("--" + name, required=True)
    args = p.parse_args()
    try:
        result = check(args.repo, args.authority, args.receipt, args.review_sha256,
                       args.operation, args.context)
    except (Rejected, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        print(json.dumps({"result": "Rejected", "reason": str(exc)}))
        return 1
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
