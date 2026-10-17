# SPDX-License-Identifier: GPL-2.0-or-later
"""Export-only research redaction. Never run against the source repository.

This tooling is intentionally outside the plugin/documentation export selection.
Copyright/SPDX notices must not be redacted: unexpected matches fail closed.
"""
import json
import os
import re
from pathlib import Path

FORBIDDEN = re.compile(r"total[\s_-]*commander|totalcmd|ghisler|\btc\b", re.I)
ARCHIVE = "https://github.com/majklvi/salamander/tree/af0e81bc1bccb6c733025fe31a97d53adf5cb32f/doc"
OLD_NAME = "directory-sync-plugin-tc-observations.md"
NEW_NAME = "directory-sync-plugin-reference-observations.md"
LINE_RULES = {}
SELECTION = {}
if os.environ.get("DIRSYNC_EXPORT_LINE_RULES"):
    LINE_RULES = json.loads(Path(os.environ["DIRSYNC_EXPORT_LINE_RULES"]).read_text(encoding="utf-8"))
if os.environ.get("DIRSYNC_EXPORT_SELECTION"):
    SELECTION = json.loads(Path(os.environ["DIRSYNC_EXPORT_SELECTION"]).read_text(encoding="utf-8"))


def filter_blob(value):
    # Path filtering happens after blob parsing in git-filter-repo. Never rewrite
    # unrelated host blobs that are being discarded from the exported history.
    if value.original_id.decode() in SELECTION["objects"]:
        value.data = blob(value.data)


def filter_commit(value):
    if value.original_id.decode() in SELECTION["commits"]:
        value.message = blob(value.message)


def filename(value):
    if value is None:
        return None  # fast-export deleteall record
    result = value.replace(OLD_NAME.encode(), NEW_NAME.encode())
    if FORBIDDEN.search(result.decode("utf-8")):
        raise ValueError("Unreviewed historical path: " + repr(value))
    return result


def text(value):
    if not FORBIDDEN.search(value):
        return value
    lines = []
    for line in value.splitlines(keepends=True):
        ending = "\r\n" if line.endswith("\r\n") else "\n" if line.endswith("\n") else ""
        body = line[:-len(ending)] if ending else line
        if FORBIDDEN.search(body) and re.search(r"copyright|SPDX|all rights reserved", body, re.I):
            raise ValueError("Review attribution before redacting: " + body)
        if body in LINE_RULES:
            lines.append(LINE_RULES[body] + ending)
            continue
        body = body.replace(OLD_NAME, NEW_NAME)
        body = re.sub(r"\[([^\]]+)\]\(https?://(?:www\.)?ghisler\.(?:ch|com)/[^\s)]+\)",
                      lambda m: "[" + m[1] + "](" + ARCHIVE + ")", body, flags=re.I)
        body = re.sub(r"https?://(?:www\.)?ghisler\.(?:ch|com)/[^\s)>]+", ARCHIVE, body, flags=re.I)
        body = body.replace(r"C:\Program Files\totalcmd\TOTALCMD.CHM", "<nápověda referenčního správce>")
        body = body.replace("tc-sync-probe-", "reference-sync-probe-")
        body = body.replace("Total Commanderu", "referenčního správce souborů")
        body = body.replace("Total Commanderem", "referenčním správcem souborů")
        body = re.sub(r"total[\s_-]*commander\w*", "referenční správce souborů", body, flags=re.I)
        body = re.sub(r"\bTC\b", "referenční program", body, flags=re.I)
        body = body.replace("totalcmd", "reference-manager").replace("TOTALCMD", "REFERENCE-MANAGER")
        for before, after in {
            "v referenčního správce": "v referenčním správci",
            "podle referenční program": "podle předlohy",
            "s referenční program": "s předlohou",
            "v referenční program": "v referenčním programu",
            "V referenční program": "V referenčním programu",
            "Autor referenční program": "Autor předlohy",
            "autora referenční program": "autora předlohy",
            "nápovědy referenční program": "nápovědy předlohy",
            "test referenční program": "test předlohy",
            "konfigurace referenční program": "konfigurace předlohy",
            "profilů referenční program": "profilů předlohy",
            "proti referenční program": "proti předloze",
            "asymetrii referenční program": "asymetrii předlohy",
            "asymetrie referenční program": "asymetrie předlohy",
            "Asymetrie referenční program": "Asymetrie předlohy",
            "protokol referenční program": "protokol předlohy",
            "ve stylu referenční program": "ve stylu předlohy",
            "kombinací referenční program": "kombinací předlohy",
            "vydání referenční program": "vydání referenčního programu",
            "dialogu referenční program": "dialogu předlohy",
            "eight referenční program combinations": "eight comparison option combinations",
        }.items():
            body = body.replace(before, after)
        if FORBIDDEN.search(body):
            raise ValueError("Unreviewed research wording: " + body)
        lines.append(body + ending)
    return "".join(lines)


def blob(data):
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        # Preserve BOM and byte order rather than transcoding historical resources.
        encoding = "utf-16-le" if data.startswith(b"\xff\xfe") else "utf-16-be"
        return data[:2] + text(data[2:].decode(encoding)).encode(encoding)
    if b"\0" in data:
        # Binary data is never rewritten. Independent post-export audit scans strings.
        return data
    try:
        original = data.decode("utf-8")
        result = text(original)
        if result != original and result.startswith("#") and "Redakční příprava exportu" not in result:
            end = result.find("\n") + 1
            result = result[:end] + (
                "\n> Redakční příprava exportu: neutrální označení předlohy a zdrojů; "
                "cesty s prefixem `reference-sync-probe-` jsou dokumentační aliasy. "
                "Výsledky a hashe se nemění; původní podklady zachovává archiv forku.\n"
            ) + result[end:]
        return result.encode("utf-8")
    except UnicodeDecodeError:
        # These are legacy localized resources; no historical redaction was needed.
        # Any ASCII product marker in unknown encoding must be explicitly reviewed.
        if FORBIDDEN.search(data.decode("ascii", errors="replace")):
            raise ValueError("Unreviewed text encoding")
        return data
