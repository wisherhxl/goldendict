"""Exercise installed OpenSpec in disposable directories; never real remotes."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    cli = shutil.which("openspec.cmd") or shutil.which("openspec")
    if not cli:
        raise RuntimeError("OpenSpec CLI required")
    with tempfile.TemporaryDirectory(prefix="gd-openspec-") as tmp:
        root = Path(tmp)
        log = []

        def run(*args, success=True):
            p = subprocess.run([cli, *args], cwd=root, capture_output=True, text=True,
                               encoding="utf-8", timeout=90)
            log.append({"args": args, "exit": p.returncode, "stdout": p.stdout, "stderr": p.stderr})
            if success and p.returncode:
                raise RuntimeError(json.dumps(log, indent=2))
            return p

        run("init", "--tools", "none")
        run("new", "change", "parity-fixture")
        change = root / "openspec/changes/parity-fixture"
        metadata = change / ".openspec.yaml"
        metadata.write_text(metadata.read_text() + "\nskip_specs: true\n")
        (change / "proposal.md").write_text("## Why\nConform to existing CRD-LOOKUP-001; no new requirement.\n\n## What Changes\nRepair existing behavior.\n\n## Capabilities\nNo requirement delta.\n\n## Impact\nFixture only.\n")
        (change / "tasks.md").write_text("## Implementation\n- [x] Verify existing requirement using fixture evidence.\n")
        status = json.loads(run("status", "--change", "parity-fixture", "--json").stdout)
        assert any(x["id"] == "specs" and x["status"] == "skipped" for x in status["artifacts"])
        run("instructions", "apply", "--change", "parity-fixture", "--json")
        run("validate", "parity-fixture", "--strict")
        before = list((root / "openspec/specs").rglob("spec.md"))
        run("archive", "parity-fixture", "--yes", "--skip-specs", "--json")
        assert not change.exists()
        archived = list((root / "openspec/changes/archive").glob("*-parity-fixture"))
        assert len(archived) == 1 and (archived[0] / "tasks.md").exists()
        assert before == list((root / "openspec/specs").rglob("spec.md"))
        # A failed candidate must retain its receipt. This is a simulated outcome,
        # not an independent real delivery verdict.
        failed = root / "failed-receipt.json"
        failed.write_text('{"fixture":true,"result":"Fail","delivered":false}')
        result = run("status", "--change", "parity-fixture", "--json", success=False)
        assert result.returncode != 0, "Re-evaluate native archived-change support"
        # Do not invent a reopening/sync rollback framework. Prove the fallback:
        # keep change active through candidate review, repair there, archive once.
        run("new", "change", "repair-fixture")
        repair = root / "openspec/changes/repair-fixture"
        meta = repair / ".openspec.yaml"
        meta.write_text(meta.read_text() + "\nskip_specs: true\n")
        (repair / "proposal.md").write_text((archived[0] / "proposal.md").read_text())
        (repair / "tasks.md").write_text("## Implementation\n- [ ] Correct review finding.\n")
        run("instructions", "apply", "--change", "repair-fixture", "--json")
        (repair / "tasks.md").write_text("## Implementation\n- [x] Correct and verify review finding.\n")
        run("validate", "repair-fixture", "--strict")
        run("archive", "repair-fixture", "--yes", "--skip-specs", "--json")
        assert json.loads(failed.read_text())["result"] == "Fail"
        assert len(list((root / "openspec/changes/archive").glob("*-repair-fixture"))) == 1
        # Exercise actual delta sync after an active-task repair. Preserve the
        # unaffected scenario and apply the corrected requirement exactly once.
        run("new", "change", "delta-repair")
        delta = root / "openspec/changes/delta-repair"
        main_spec = root / "openspec/specs/fixture/spec.md"
        main_spec.parent.mkdir(parents=True)
        original = "# Fixture\n\n## Purpose\nA local fixture for deterministic archive synchronization and preservation checks.\n\n## Requirements\n\n### Requirement: Existing lookup\nThe fixture MUST retain the approved lookup behavior.\n\n#### Scenario: Existing lookup succeeds\n- **WHEN** lookup is requested\n- **THEN** the original result is returned\n"
        main_spec.write_text(original)
        (delta / "proposal.md").write_text("## Why\nTest a declared fixture behavior change.\n\n## What Changes\nReturn the corrected result.\n\n## Capabilities\n### Modified Capabilities\n- fixture: Correct the result.\n\n## Impact\nTemporary fixture only.\n")
        (delta / "tasks.md").write_text("## Implementation\n- [ ] Repair simulated failed review.\n")
        delta_spec = delta / "specs/fixture/spec.md"
        delta_spec.parent.mkdir(parents=True)
        delta_spec.write_text(original[original.index("### Requirement:"):].replace("original result", "incorrect result").join(["## MODIFIED Requirements\n\n", ""]))
        run("instructions", "apply", "--change", "delta-repair", "--json")
        assert main_spec.read_text() == original  # No premature sync after failure.
        delta_spec.write_text(delta_spec.read_text().replace("incorrect result", "corrected result"))
        (delta / "tasks.md").write_text("## Implementation\n- [x] Repair and verify corrected result.\n")
        run("validate", "delta-repair", "--strict")
        run("archive", "delta-repair", "--yes", "--json")
        synced = main_spec.read_text()
        assert synced.count("### Requirement: Existing lookup") == 1
        assert "corrected result" in synced and "incorrect result" not in synced
        assert "Existing lookup succeeds" in synced
        assert failed.exists() and not delta.exists()
        print(json.dumps({"result": "Passed", "archive_policy": "lightweight-post-review-closeout",
                          "reason": "native status/apply cannot resolve the archived active change name",
                          "product_verified": False, "commands": log}, indent=2))


if __name__ == "__main__":
    main()
