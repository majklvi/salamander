# SPDX-License-Identifier: GPL-2.0-or-later
"""Independent read-only Git object auditor; no reliance on filter callbacks."""
import argparse
import json
import re
import subprocess
from pathlib import Path

PATTERN = re.compile(r"total[\s_-]*commander|totalcmd|ghisler|\btc\b", re.I)


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], timeout=110)


def content_hits(data):
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return bool(PATTERN.search(data.decode("utf-16")))
    if b"\0" not in data:
        return bool(PATTERN.search(data.decode("utf-8", errors="replace")))
    # Audit ASCII/UTF-16 strings in icons/resources without interpreting arbitrary
    # binary byte pairs as the standalone two-letter abbreviation.
    for encoding in ("utf-8", "utf-16-le", "utf-16-be"):
        decoded = data.decode(encoding, errors="replace")
        for printable in re.findall(r"[\x20-\x7e]{8,}", decoded):
            if PATTERN.search(printable):
                return True
    return False


def audit(repo):
    repo = Path(repo).resolve()
    refs = git(repo, "for-each-ref", "--format=%(refname)").decode().splitlines()
    assert refs, "No refs to audit"
    failures = [{"kind": "ref", "value": ref} for ref in refs if PATTERN.search(ref)]
    objects = [line.split(b" ", 1)[0] for line in git(repo, "rev-list", "--objects", "--all").splitlines()]
    objects += git(repo, "for-each-ref", "--format=%(objectname)").splitlines()
    objects = sorted(set(objects))
    output = subprocess.run(["git", "-C", str(repo), "cat-file", "--batch"],
                            input=b"\n".join(objects) + b"\n", stdout=subprocess.PIPE,
                            check=True, timeout=110).stdout
    counts = {"blob": 0, "commit": 0, "tree": 0, "tag": 0}
    offset = 0
    for sha in objects:
        end = output.index(b"\n", offset)
        header = output[offset:end].split()
        kind = header[1].decode()
        size = int(header[2])
        data = output[end+1:end+1+size]
        offset = end + size + 2
        counts[kind] += 1
        if kind in ("blob", "commit", "tag") and content_hits(data):
            failures.append({"kind": kind, "object": sha.decode()})
    # Explicitly enumerate every historical tree path, including renamed/deleted paths.
    path_count = 0
    for sha in git(repo, "rev-list", "--all").splitlines():
        paths = git(repo, "ls-tree", "-r", "--name-only", "-z", sha.decode()).split(b"\0")
        for path in filter(None, paths):
            path_count += 1
            if PATTERN.search(path.decode("utf-8")):
                failures.append({"kind": "path", "commit": sha.decode(), "path": path.decode()})
    alternates = Path(git(repo, "rev-parse", "--absolute-git-dir").decode().strip()) / "objects/info/alternates"
    assert not alternates.exists(), "Object database must not depend on the source repository"
    return {"head": git(repo, "rev-parse", "HEAD").decode().strip(), "refs": refs,
            "objects": counts, "historicalPathOccurrences": path_count,
            "remoteNames": git(repo, "remote").decode().splitlines(), "failures": failures,
            "pass": not failures}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = audit(args.repository)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["pass"] else 1)
