#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Run and compare resumable real-dictionary lifecycle observations."""

from __future__ import annotations

import argparse
import os
import stat
import tempfile
from collections.abc import Iterable
from pathlib import Path

import real_dictionary_acceptance_result as acceptance_result
import real_dictionary_acceptance_workspace as acceptance_workspace

SCENARIOS = ("clean-discovery", "warm-restart", "explicit-rescan")
SCENARIO_TOKEN = "__GOLDENDICT_ACCEPTANCE_SCENARIO__"
SUMMARY_SCHEMA = "goldendict-real-dictionary-lifecycle-summary-v1"


class LifecycleError(RuntimeError):
    """Raised when lifecycle evidence is incomplete or inconsistent."""


def _is_reparse_point(path: Path) -> bool:
    try:
        attributes = getattr(path.stat(follow_symlinks=False), "st_file_attributes", 0)
    except OSError as error:
        raise LifecycleError(f"Cannot inspect lifecycle output: {path}") from error
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return bool(attributes & reparse_flag)


def _confined_output_directory(parent: Path, name: str, label: str) -> Path:
    try:
        parent_root = parent.resolve(strict=True)
    except OSError as error:
        raise LifecycleError(f"Cannot resolve {label} parent: {parent}") from error
    candidate = parent_root / name
    if candidate.is_symlink() or (candidate.exists() and _is_reparse_point(candidate)):
        raise LifecycleError(f"{label} directory must not be a link or reparse point")
    try:
        candidate.mkdir()
    except FileExistsError:
        if not candidate.is_dir():
            raise LifecycleError(f"{label} path is not a directory") from None
    except OSError as error:
        raise LifecycleError(f"Cannot create {label} directory: {candidate}") from error
    try:
        resolved = candidate.resolve(strict=True)
        resolved.relative_to(parent_root)
    except (OSError, ValueError) as error:
        raise LifecycleError(f"{label} directory escapes its workspace root") from error
    if resolved != candidate:
        raise LifecycleError(f"{label} directory is not a direct workspace child")
    return resolved


def _clear_confined_directory(directory: Path, label: str) -> None:
    files: list[Path] = []
    directories: list[Path] = []

    def inspect(parent: Path) -> None:
        try:
            entries = list(os.scandir(parent))
        except OSError as error:
            raise LifecycleError(
                f"Cannot inspect {label} directory: {parent}"
            ) from error
        for entry in entries:
            path = Path(entry.path)
            if entry.is_symlink() or _is_reparse_point(path):
                raise LifecycleError(
                    f"{label} directory contains a link or reparse point"
                )
            try:
                metadata = entry.stat(follow_symlinks=False)
            except OSError as error:
                raise LifecycleError(f"Cannot inspect {label} entry: {path}") from error
            if stat.S_ISDIR(metadata.st_mode):
                inspect(path)
                directories.append(path)
            elif stat.S_ISREG(metadata.st_mode):
                files.append(path)
            else:
                raise LifecycleError(f"{label} contains a non-regular entry")

    inspect(directory)
    try:
        for path in files:
            path.unlink()
        for path in directories:
            path.rmdir()
    except OSError as error:
        raise LifecycleError(f"Cannot reset {label} directory") from error


def _reset_clean_run_state(workspace_root: Path, version: str) -> None:
    version_root = workspace_root / version
    for name in acceptance_workspace.DIRECTORY_NAMES:
        if name == "evidence":
            continue
        directory = _confined_output_directory(version_root, name, f"Clean-run {name}")
        _clear_confined_directory(directory, f"Clean-run {name}")


def _scenario_command(command: list[str], scenario: str) -> list[str]:
    if command.count(SCENARIO_TOKEN) != 1:
        raise LifecycleError(
            f"Child command must contain exactly one {SCENARIO_TOKEN} token"
        )
    return [scenario if item == SCENARIO_TOKEN else item for item in command]


def _expected_revision(version: str, qt5_revision: str, qt6_revision: str) -> str:
    return qt5_revision if version == "qt5" else qt6_revision


def _read_archives(
    directory: Path,
    *,
    pair_id: str,
    version: str,
    revision: str,
    manifest_hash: str,
    conditions_hash: str,
) -> list[dict[str, object]]:
    observations: list[dict[str, object]] = []
    missing_seen = False
    for scenario in SCENARIOS:
        path = directory / f"{scenario}.json"
        if not path.exists():
            missing_seen = True
            continue
        if missing_seen:
            raise LifecycleError("Lifecycle evidence contains a scenario gap")
        if path.is_symlink() or _is_reparse_point(path) or not path.is_file():
            raise LifecycleError("Lifecycle archive must be a regular workspace file")
        observation = acceptance_result.read_observation(
            path,
            expected_pair_id=pair_id,
            expected_version=version,
            expected_revision=revision,
            expected_manifest_hash=manifest_hash,
            expected_conditions_hash=conditions_hash,
        )
        if observation["scenario"] != scenario:
            raise LifecycleError("Lifecycle archive scenario does not match its name")
        observations.append(observation)
    _validate_transitions(observations)
    return observations


def _index_dispositions(observation: dict[str, object]) -> set[str]:
    return {str(item["disposition"]) for item in observation["indexes"]}


def _index_identities(
    observation: dict[str, object],
) -> set[tuple[str, str, str, str, int]]:
    return {
        (
            str(item["dictionary_key"]),
            str(item["role"]),
            str(item["file_name"]),
            str(item["sha256"]),
            int(item["size"]),
        )
        for item in observation["indexes"]
    }


def _validate_transitions(observations: list[dict[str, object]]) -> None:
    if not observations:
        return
    first = observations[0]
    if first["scenario"] != "clean-discovery":
        raise LifecycleError("Lifecycle evidence must start with clean discovery")
    dictionaries = first["dictionaries"]
    index_identities = _index_identities(first)
    if not index_identities or _index_dispositions(first) != {"created"}:
        raise LifecycleError("Clean discovery must create at least one index")
    for index, observation in enumerate(observations):
        if observation["scenario"] != SCENARIOS[index]:
            raise LifecycleError("Lifecycle scenarios are not contiguous")
        if observation["dictionaries"] != dictionaries:
            raise LifecycleError(
                "Dictionary identity changed across lifecycle scenarios"
            )
        if _index_identities(observation) != index_identities:
            raise LifecycleError("Index identity changed across lifecycle scenarios")
        if index > 0 and _index_dispositions(observation) - {"reused"}:
            raise LifecycleError(
                "An unchanged lifecycle scenario did not reuse indexes"
            )


def _archive_observation(source: Path, destination: Path) -> dict[str, object]:
    observation = acceptance_result.read_observation(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    acceptance_result.write_observation(destination, observation)
    return observation


def run_version(
    workspace: Path,
    corpus: Path,
    manifest: Path,
    conditions: Path,
    qt5_revision: str,
    qt6_revision: str,
    version: str,
    command: list[str],
) -> list[Path]:
    pair_id = acceptance_workspace.validate_workspace(
        workspace,
        corpus,
        manifest,
        conditions,
        qt5_revision,
        qt6_revision,
    )
    if version not in acceptance_result.VERSIONS:
        raise LifecycleError("Lifecycle version must be qt5 or qt6")
    _scenario_command(command, SCENARIOS[0])
    workspace_root = workspace.resolve(strict=True)
    revision = _expected_revision(version, qt5_revision, qt6_revision)
    evidence_root = workspace_root / version / "evidence"
    observation_path = evidence_root / "observation.json"
    archive_root = _confined_output_directory(
        evidence_root, "lifecycle", "Lifecycle archive"
    )
    manifest_hash, conditions_hash = acceptance_workspace.read_pair_hashes(
        workspace_root, pair_id
    )
    archives = _read_archives(
        archive_root,
        pair_id=pair_id,
        version=version,
        revision=revision,
        manifest_hash=manifest_hash,
        conditions_hash=conditions_hash,
    )

    if not archives:
        _reset_clean_run_state(workspace_root, version)

    for scenario in SCENARIOS[len(archives) :]:
        return_code = acceptance_workspace.run_in_workspace(
            workspace,
            corpus,
            manifest,
            conditions,
            qt5_revision,
            qt6_revision,
            version,
            _scenario_command(command, scenario),
            require_result=True,
        )
        if return_code != 0:
            raise LifecycleError(
                f"{version} {scenario} failed with exit code {return_code}"
            )
        observation = acceptance_result.read_observation(
            observation_path,
            expected_pair_id=pair_id,
            expected_version=version,
            expected_revision=revision,
        )
        if observation["scenario"] != scenario:
            raise LifecycleError("Observer published the wrong lifecycle scenario")
        archives.append(observation)
        _validate_transitions(archives)
        _archive_observation(observation_path, archive_root / f"{scenario}.json")

    return [archive_root / f"{scenario}.json" for scenario in SCENARIOS]


def _atomic_summary(path: Path, value: dict[str, object]) -> None:
    content = acceptance_result.canonical_json(_validate_summary(value))
    temporary: Path | None = None
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{path.name}.", dir=path.parent
        )
        temporary = Path(temporary_name)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except OSError as error:
        raise LifecycleError(f"Cannot publish lifecycle summary: {path}") from error
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _validate_summary(value: object) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != {
        "equivalent",
        "pair_id",
        "scenarios",
        "schema",
        "total_difference_count",
    }:
        raise LifecycleError("Lifecycle summary has an unexpected structure")
    if (
        value["schema"] != SUMMARY_SCHEMA
        or not isinstance(value["pair_id"], str)
        or not acceptance_result.HASH_PATTERN.fullmatch(value["pair_id"])
    ):
        raise LifecycleError("Lifecycle summary identity is invalid")
    records = value["scenarios"]
    if not isinstance(records, list) or len(records) != len(SCENARIOS):
        raise LifecycleError("Lifecycle summary must contain every scenario")
    total = 0
    for index, record in enumerate(records):
        if not isinstance(record, dict) or set(record) != {
            "comparison",
            "difference_count",
            "equivalent",
            "scenario",
        }:
            raise LifecycleError("Lifecycle summary scenario is malformed")
        scenario = SCENARIOS[index]
        count = record["difference_count"]
        if (
            record["scenario"] != scenario
            or record["comparison"] != f"comparisons/{scenario}.json"
            or not isinstance(count, int)
            or isinstance(count, bool)
            or count < 0
            or record["equivalent"] != (count == 0)
        ):
            raise LifecycleError("Lifecycle summary scenario is inconsistent")
        total += count
    if value["total_difference_count"] != total or value["equivalent"] != (total == 0):
        raise LifecycleError("Lifecycle summary totals are inconsistent")
    return value


def compare_lifecycle(
    workspace: Path,
    corpus: Path,
    manifest: Path,
    conditions: Path,
    qt5_revision: str,
    qt6_revision: str,
) -> Path:
    pair_id = acceptance_workspace.validate_workspace(
        workspace,
        corpus,
        manifest,
        conditions,
        qt5_revision,
        qt6_revision,
    )
    workspace_root = workspace.resolve(strict=True)
    manifest_hash, conditions_hash = acceptance_workspace.read_pair_hashes(
        workspace_root, pair_id
    )
    archives = {
        version: _read_archives(
            workspace_root / version / "evidence" / "lifecycle",
            pair_id=pair_id,
            version=version,
            revision=_expected_revision(version, qt5_revision, qt6_revision),
            manifest_hash=manifest_hash,
            conditions_hash=conditions_hash,
        )
        for version in acceptance_result.VERSIONS
    }
    if any(len(values) != len(SCENARIOS) for values in archives.values()):
        raise LifecycleError("Both versions must complete every lifecycle scenario")

    comparison_root = _confined_output_directory(
        workspace_root, "comparisons", "Lifecycle comparison"
    )
    records: list[dict[str, object]] = []
    total_differences = 0
    for index, scenario in enumerate(SCENARIOS):
        comparison = acceptance_result.compare_observations(
            archives["qt5"][index], archives["qt6"][index]
        )
        output = comparison_root / f"{scenario}.json"
        acceptance_result.write_comparison(output, comparison)
        difference_count = int(comparison["difference_count"])
        total_differences += difference_count
        records.append(
            {
                "comparison": f"comparisons/{scenario}.json",
                "difference_count": difference_count,
                "equivalent": comparison["equivalent"],
                "scenario": scenario,
            }
        )
    summary = {
        "equivalent": total_differences == 0,
        "pair_id": pair_id,
        "scenarios": records,
        "schema": SUMMARY_SCHEMA,
        "total_difference_count": total_differences,
    }
    summary_path = comparison_root / "lifecycle-summary.json"
    _atomic_summary(summary_path, summary)
    return summary_path


def _common_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--conditions", type=Path, required=True)
    parser.add_argument("--qt5-revision", required=True)
    parser.add_argument("--qt6-revision", required=True)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    run = subparsers.add_parser("run-version")
    _common_arguments(run)
    run.add_argument("--version", choices=acceptance_result.VERSIONS, required=True)
    run.add_argument("child_command", nargs=argparse.REMAINDER)
    compare = subparsers.add_parser("compare")
    _common_arguments(compare)
    return parser


def main(arguments: Iterable[str] | None = None) -> int:
    options = _parser().parse_args(arguments)
    try:
        if options.command == "run-version":
            child_command = options.child_command
            if child_command and child_command[0] == "--":
                child_command = child_command[1:]
            outputs = run_version(
                options.workspace,
                options.corpus,
                options.manifest,
                options.conditions,
                options.qt5_revision,
                options.qt6_revision,
                options.version,
                child_command,
            )
            print("\n".join(str(path) for path in outputs))
        else:
            print(
                compare_lifecycle(
                    options.workspace,
                    options.corpus,
                    options.manifest,
                    options.conditions,
                    options.qt5_revision,
                    options.qt6_revision,
                )
            )
    except (
        LifecycleError,
        acceptance_result.ResultError,
        acceptance_workspace.WorkspaceError,
    ) as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
