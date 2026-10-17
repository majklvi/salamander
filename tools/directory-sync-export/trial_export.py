# SPDX-License-Identifier: GPL-2.0-or-later
"""Local-only export rehearsal. Requires an already installed, pinned tool runtime.

No push, network operation, source mutation or recursive deletion is performed.
The destination must be new and below the explicitly supplied local evidence root.
"""
import argparse
import difflib
import hashlib
import importlib.metadata
import json
import os
import subprocess
import sys
from pathlib import Path

import audit_export
import neutral_filter


def run(args, cwd, **kwargs):
    return subprocess.run(args, cwd=cwd, check=True, timeout=110, **kwargs)


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], timeout=110)


def owned(path):
    return path.startswith("src/plugins/dirsync/") or path.startswith("doc/directory-sync-plugin")


def tree(repo, commit):
    result = {}
    for entry in git(repo, "ls-tree", "-r", "-z", commit).split(b"\0"):
        if not entry:
            continue
        meta, path = entry.split(b"\t", 1)
        mode, kind, sha = meta.decode().split()
        if kind != "blob":
            if owned(path.decode("utf-8")):
                raise ValueError("Unexpected selected non-blob entry: " + repr(entry))
            continue  # Unrelated host submodules are outside the selected history.
        result[path.decode("utf-8")] = (mode, sha)
    return result


def sha_blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def read_objects(repo, ids):
    output = subprocess.run(["git", "-C", str(repo), "cat-file", "--batch"],
                            input=("\n".join(ids) + "\n").encode(), stdout=subprocess.PIPE,
                            check=True, timeout=110).stdout
    result, offset = {}, 0
    for sha in ids:
        end = output.index(b"\n", offset)
        size = int(output[offset:end].split()[2])
        result[sha] = output[end+1:end+1+size]
        offset = end + size + 2
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--tool-runtime", type=Path, required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--preparation-commit", default="3552fafbec51af9f47044f9c33484660a0643aa2",
                        help="Committed, reviewed neutral-text revision used to derive literal line rules")
    parser.add_argument("--resume-filtered", action="store_true",
                        help="Resume verification of this trial's completed filter; never re-filter or overwrite it")
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    evidence = args.evidence_root.resolve(strict=True)
    target = (evidence / args.name).resolve()
    if target.parent != evidence or (target.exists() and not args.resume_filtered) or source == target or source in target.parents:
        raise ValueError("Destination must be a new direct child of the separate evidence root")
    if args.resume_filtered and not (target / ".git/filter-repo/commit-map").is_file():
        raise ValueError("Resume requires an existing completed filter and commit map")
    head = args.head
    installed_versions = {d.metadata["Name"]: d.version for d in importlib.metadata.distributions(path=[str(args.tool_runtime)])}
    assert installed_versions.get("git-filter-repo") == "2.47.0", "Use pinned git-filter-repo 2.47.0"
    assert git(source, "rev-parse", "HEAD").decode().strip() == head
    assert git(source, "branch", "--show-current").decode().strip() == "feature/directory-sync-plugin"
    before_refs = git(source, "show-ref")
    before_status = git(source, "status", "--porcelain", "-z")
    original_files = tree(source, head)
    manifest = json.loads(git(source, "show", head + ":doc/directory-sync-plugin-repository-manifest.json"))
    own_commits = git(source, "rev-list", "--reverse", "samandarin-0.16.." + head).decode().splitlines()
    # Reviewed line replacements come from the committed neutral-document preparation.
    rules = {}
    prepared_head = args.preparation_commit
    old_head = git(source, "rev-parse", prepared_head + "^").decode().strip()
    for path in tree(source, prepared_head):
        if not owned(path):
            continue
        old_path = path.replace(neutral_filter.NEW_NAME, neutral_filter.OLD_NAME)
        old = subprocess.run(["git", "-C", str(source), "show", old_head + ":" + old_path],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=110)
        new = git(source, "show", prepared_head + ":" + path)
        if old.returncode or b"\0" in new:
            continue
        try:
            a, b = old.stdout.decode("utf-8").splitlines(), new.decode("utf-8").splitlines()
        except UnicodeDecodeError:
            continue
        for operation, i, j, k, l in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
            if operation == "replace" and j-i == l-k:
                for prior, updated in zip(a[i:j], b[k:l]):
                    if neutral_filter.FORBIDDEN.search(prior) and not neutral_filter.FORBIDDEN.search(updated):
                        assert prior not in rules or rules[prior] == updated
                        rules[prior] = updated
    rule_file = evidence / (args.name + "-line-rules.json")
    rule_file.write_text(json.dumps(rules, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="")
    neutral_filter.LINE_RULES = rules
    selection_file = evidence / (args.name + "-selection.json")
    selected_objects = [line.split(b" ", 1)[0].decode() for line in git(
        source, "rev-list", "--objects", head, "--", "src/plugins/dirsync/",
        ":(glob)doc/directory-sync-plugin*").splitlines()]
    selection_file.write_text(json.dumps({"objects": selected_objects, "commits": own_commits}),
                              encoding="utf-8", newline="")
    env = os.environ.copy()
    env["PYTHONPATH"] = os.pathsep.join([str(args.tool_runtime.resolve()), str(Path(__file__).parent.resolve())])
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    env["DIRSYNC_EXPORT_LINE_RULES"] = str(rule_file)
    env["DIRSYNC_EXPORT_SELECTION"] = str(selection_file)
    # --no-local avoids hardlinks, alternates, shared packs and source ref mutation.
    if not args.resume_filtered:
        run(["git", "-c", "core.longpaths=true", "clone", "--no-local", "--no-tags", "--single-branch", "--branch",
             "feature/directory-sync-plugin", str(source), str(target)], evidence)
        assert git(target, "rev-parse", "HEAD").decode().strip() == head
    filter_args = [sys.executable, "-m", "git_filter_repo", "--path", "src/plugins/dirsync/",
                   "--path-glob", "doc/directory-sync-plugin*", "--replace-refs", "delete-no-add",
                   "--preserve-commit-hashes", "--filename-callback",
                   "import neutral_filter; return neutral_filter.filename(filename)",
                   "--blob-callback", "import neutral_filter; neutral_filter.filter_blob(blob)",
                   "--commit-callback", "import neutral_filter; neutral_filter.filter_commit(commit)"]
    if not args.resume_filtered:
        run(filter_args, target, env=env)
    assert not git(target, "remote").strip(), "Filtered repository must have no remote"
    commit_map_file = target / ".git/filter-repo/commit-map"
    commit_map = dict(line.split() for line in commit_map_file.read_text().splitlines()[1:])
    assert git(target, "rev-parse", "HEAD").decode().strip() == commit_map[head]
    source_objects = read_objects(source, selected_objects)
    target_commits = read_objects(target, [commit_map[c] for c in own_commits])
    blobs, redactions = {}, []
    verified = 0
    # Verify every original plugin revision, not just the tip: content, mode,
    # path selection, author/committer identities and exact epoch/timezone values.
    for commit in own_commits:
        mapped = commit_map[commit]
        assert mapped != "0" * 40, "Lost plugin commit: " + commit
        expected = {}
        for path, (mode, sha) in tree(source, commit).items():
            if not owned(path):
                continue
            new_path = neutral_filter.filename(path.encode()).decode()
            assert new_path not in expected, "Path collision: " + new_path
            if sha not in blobs:
                data = source_objects[sha]
                updated = neutral_filter.blob(data)
                blobs[sha] = sha_blob(updated)
                if data != updated:
                    if path.startswith("src/"):
                        assert data.replace(b"eight TC combinations", b"eight comparison option combinations") == updated
                    redactions.append({"old": sha, "new": blobs[sha], "examplePath": path})
            expected[new_path] = (mode, blobs[sha])
        assert tree(target, mapped) == expected, "Unexpected historical tree: " + commit
        for field in ("author", "committer"):
            def metadata(objects, sha):
                raw = objects[sha].split(b"\n\n", 1)[0]
                return next(line for line in raw.splitlines() if line.startswith(field.encode() + b" "))
            assert metadata(source_objects, commit) == metadata(target_commits, mapped), (commit, field)
        assert neutral_filter.blob(source_objects[commit].split(b"\n\n", 1)[1]) == target_commits[mapped].split(b"\n\n", 1)[1]
        verified += 1
    # Only the committed diagnostic label differs at the filtered tip.
    tip_check = {p: sha for p, (_, sha) in tree(target, "HEAD").items() if p.startswith("src/")}
    assert all(original_files[p][1] == sha for p, sha in tip_check.items())
    # Rehearse vendoring without changing include/build paths: this is not a build migration.
    support = []
    for entry in manifest["vendorSnapshot"]:
        data = git(source, "show", head + ":" + entry["source"])
        # The original manifest hashed worktree bytes, which may use CRLF. Export
        # canonical Git blob bytes; validate the recorded hash without changing data.
        raw_hash = hashlib.sha256(data).hexdigest()
        crlf_hash = hashlib.sha256(data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")).hexdigest()
        assert entry["sha256"] in (raw_hash, crlf_hash), entry["source"]
        updated = data
        if entry["source"] == "src/plugins/shared/spl_com.h":
            updated = data.replace(b"(including Total Commander proxy builds)", b"(including proxy builds)")
            assert updated != data
        p = target / entry["proposedTarget"]
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(updated)
        support.append({"source": entry["source"], "target": entry["proposedTarget"],
                        "sourceSha256": hashlib.sha256(data).hexdigest(),
                        "originalManifestWorktreeSha256": entry["sha256"],
                        "exportedSha256": hashlib.sha256(updated).hexdigest(), "changed": data != updated})
    (target / "LICENSE").write_bytes(git(source, "show", head + ":LICENSE"))
    (target / "EXPORT-ATTRIBUTIONS.md").write_text(
        "# Export rehearsal provenance\n\nLocal trial only; build layout has not been migrated.\n\n"
        "SDK/UI code: Open Salamander / Samandarin contributors; original notices retained.\n"
        "Appearance library: darkmodelib by ozone10; bundled notices and licenses retained.\n"
        "Reference file manager and its community: behavior/usability research only; no code, artwork "
        "or help copied. Original research credits: [source archive](" + neutral_filter.ARCHIVE + "/third_party.md).\n",
        encoding="utf-8", newline="")
    (target / "EXPORT-SUPPORT.json").write_text(json.dumps(support, indent=2) + "\n", encoding="utf-8", newline="")
    run(["git", "add", "--", "vendor", "LICENSE", "EXPORT-ATTRIBUTIONS.md", "EXPORT-SUPPORT.json"], target)
    local_env = env.copy()
    local_env.update(GIT_AUTHOR_DATE="2026-10-17T12:00:00+02:00", GIT_COMMITTER_DATE="2026-10-17T12:00:00+02:00")
    run(["git", "-c", "user.name=Local export validation", "-c", "user.email=export-validation@localhost",
         "commit", "-m", "Add pinned support snapshots for local export validation"], target, env=local_env)
    audit = audit_export.audit(target)
    assert audit["pass"], audit["failures"]
    assert not audit["remoteNames"] and not git(target, "status", "--porcelain").strip()
    run(["git", "fsck", "--full", "--no-reflogs"], target)
    # Deterministic bytes of source refs/status prove no source mutation occurred.
    assert git(source, "show-ref") == before_refs
    assert git(source, "status", "--porcelain", "-z") == before_status
    result = {"sourceHead": head, "target": str(target), "toolVersion": "2.47.0",
              "preparationCommit": prepared_head,
              "reviewedLineRules": len(rules), "verifiedOriginalCommits": verified,
              "mappedPluginCommits": {c: commit_map[c] for c in own_commits},
              "sourceBlobsCompared": len(blobs), "redactedBlobs": redactions,
              "vendorFiles": len(support), "changedVendorFiles": sum(e["changed"] for e in support),
              "tipProductionSourcesUnchanged": True, "sourceRefsAndStatusUnchanged": True,
              "audit": audit}
    out = evidence / (args.name + "-result.json")
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="")
    (evidence / (args.name + "-commit-map.txt")).write_bytes(commit_map_file.read_bytes())
    print(json.dumps({k: v for k, v in result.items() if k not in ("mappedPluginCommits", "redactedBlobs")}, indent=2))


if __name__ == "__main__":
    main()
