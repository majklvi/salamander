# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify a fresh local clone and demonstrate detection of historical-only markers."""
import argparse
import json
import subprocess
from pathlib import Path

import audit_export
import neutral_filter


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trial", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    parser.add_argument("--name", required=True)
    args = parser.parse_args()
    evidence = args.evidence_root.resolve(strict=True)
    fresh = (evidence / (args.name + "-fresh")).resolve()
    negative = (evidence / (args.name + "-negative")).resolve()
    for path in (fresh, negative):
        assert path.parent == evidence and not path.exists()
    def run(repo, *git_args):
        return subprocess.check_output(["git", "-C", str(repo), *git_args], timeout=110, stderr=subprocess.STDOUT)
    subprocess.run(["git", "clone", "--no-local", "--no-tags", "--single-branch",
                    str(args.trial.resolve(strict=True)), str(fresh)], check=True, timeout=110)
    # Remove the symbolic remote HEAD first; on this Windows Git build removing
    # the remote otherwise leaves a dangling symbolic ref reported by fsck.
    run(fresh, "symbolic-ref", "--delete", "refs/remotes/origin/HEAD")
    run(fresh, "remote", "remove", "origin")
    result = audit_export.audit(fresh)
    assert result["pass"] and not result["remoteNames"]
    assert run(fresh, "rev-parse", "HEAD") == run(args.trial, "rev-parse", "HEAD")
    assert not run(fresh, "status", "--porcelain").strip()
    # Ensure the source object database was not retained as unreachable objects.
    all_objects = run(fresh, "cat-file", "--batch-all-objects", "--batch-check=%(objectname)").splitlines()
    reachable = {line.split(b" ", 1)[0] for line in run(fresh, "rev-list", "--objects", "--all").splitlines()}
    assert set(all_objects) == reachable
    assert all(p.stat().st_nlink == 1 for p in (fresh / ".git/objects").rglob("*") if p.is_file())
    run(fresh, "fsck", "--full", "--no-reflogs")
    # Distinct negative repository: the final checkout contains no marker, but
    # old file bytes, deleted path, commit message, ref and annotated tag do.
    negative.mkdir()
    run(negative, "init", "-b", "main")
    run(negative, "config", "user.name", "Export audit fixture")
    run(negative, "config", "user.email", "fixture@localhost")
    (negative / "tc-old-path.txt").write_text("Total Commander historical fixture\n", encoding="utf-8")
    (negative / "wide.txt").write_bytes("Total Commander UTF-16 fixture".encode("utf-16"))
    run(negative, "add", ".")
    run(negative, "commit", "-m", "Historical reference fixture")
    run(negative, "tag", "-a", "old-evidence", "-m", "TC annotated message fixture")
    run(negative, "rm", "--", "tc-old-path.txt", "wide.txt")
    run(negative, "commit", "-m", "Remove TC fixture from current tree")
    run(negative, "branch", "tc-audit-control")
    failures = audit_export.audit(negative)
    kinds = {failure["kind"] for failure in failures["failures"]}
    assert not failures["pass"] and kinds == {"blob", "commit", "tag", "path", "ref"}, kinds
    assert not run(negative, "ls-files").strip()
    # Callback edge cases and fail-closed behavior found during the rehearsal.
    assert neutral_filter.filename(None) is None
    assert neutral_filter.filename(b"doc/" + neutral_filter.OLD_NAME.encode()).endswith(neutral_filter.NEW_NAME.encode())
    wide = "TC reference fixture".encode("utf-16")
    assert not audit_export.content_hits(neutral_filter.blob(wide))
    assert neutral_filter.blob(b"\0\x89binary") == b"\0\x89binary"
    for suspicious in ("Copyright Ghisler", "Unknown ghisler wording"):
        try:
            neutral_filter.text(suspicious)
        except ValueError:
            pass
        else:
            raise AssertionError("Expected unreviewed wording to fail closed")
    result["allStoredObjectsReachable"] = True
    result["independentObjectFiles"] = True
    result["negativeControlDetectedKinds"] = sorted(kinds)
    result["negativeControlFinalCheckoutEmpty"] = True
    result["callbackEdgeCasesPassed"] = True
    output = evidence / (args.name + "-verification.json")
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
