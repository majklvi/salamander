# Disk selection SDK and Branch View

## One selection contract for disk plug-ins

A panel path identifies the location being browsed. In Branch View it is the
scan root, while each file can have a different parent. `CFileData.Name` and
`NameW` remain basenames; joining them to `GetPanelPath()` cannot identify an
arbitrary disk item. Replacing these fields with paths would break masks,
extensions, rename rules and existing binary plug-ins.

The standard SDK facade supplies complete paths and metadata together:

```cpp
#include "spl_com.h"
#include "spl_gen.h"
#include "spl_diskselection.h"

// Capture on the host UI thread before starting a dialog or worker.
CSalamanderDiskSelection selection;
if (!selection.Capture(general, PANEL_SOURCE))
    return FALSE;
for (int i = 0; i < selection.GetCount(); ++i)
{
    const CSalamanderDiskSelectionItem* item = selection.GetItem(i);
    // NameW: basename for masks/display.
    // FullPathW: authoritative UTF-16 operation path.
    // DirectoryW: actual parent for sibling/output defaults.
    // RelativePathW: path below selection.GetRootPathW().
    // Use wide I/O and an extended prefix where needed.
}
```

The same code works for local folders, mapped drives, UNC and Branch View.
Authors do not detect Branch View, inspect private metadata, resolve borrowed
rows individually or implement a recursive fallback. DemoPlug's `.DOP File(s)`
command demonstrates capture before progress work.

The default mode captures selected items, or the focus if nothing is selected.
Other modes are `SALDISKSELECTION_SELECTED_ONLY`,
`SALDISKSELECTION_FOCUSED_ONLY` and `SALDISKSELECTION_ALL_ITEMS`. Files and
directories retain panel order; synthetic `..` is omitted. A valid empty
selection succeeds with zero items. Archive and plug-in filesystem panels
return `ERROR_NOT_SUPPORTED` and keep their existing virtual-path contracts.

## Ownership, errors and compatibility

A successful capture owns immutable copies of all paths, names and metadata.
Refresh, sorting, selection changes and source-panel destruction cannot alter
it. Its const getters can be read on worker threads after UI-thread capture.
The facade is movable and noncopyable; destruction or `Reset()` releases it.
Returned item pointers remain valid until release. Keep the owner alive, and
do not reset or move it concurrently with readers. Normal plug-in worker and
unload lifetime rules still apply.

Capture is atomic. Missing identities, wrong-thread calls, unsupported panels
and allocation failures return false with `GetLastError()`, leaving the facade
empty. Failure cannot produce a partial batch or a guessed root/basename path.
The conversion helpers can throw `std::bad_alloc`; callers must handle it before
starting work or clean up active work appropriately.

The host exposes a new versioned `Salamander.DiskSelection` service through the
existing `QueryService`. The facade handles discovery, version checks and
release. On older **Samandarin hosts with the QueryService ABI**, it captures
owned data through `PanelItemPaths`, or ordinary-folder enumeration if neither
service exists. An advertised service is authoritative, including its errors.

Newly rebuilt plug-ins are **not** promised to run on historical Open Salamander
hosts whose general-interface vtable predates QueryService. A missing vtable
slot cannot be safely probed. Existing binary plug-ins remain compatible with
the new host: published vtable prefixes and `CFileData` are unchanged and
`PluginData` is not repurposed. Old binaries that construct root/basename still
need a one-time source migration; their old filename contract cannot silently
be changed by the host.

`PanelItemPaths` remains available for current-row lookups, with constant-time
pointer-membership validation. New batch commands should use the owned
selection instead of holding borrowed panel pointers.

## Supplied consumers

| Consumer | Disk-selection boundary |
| --- | --- |
| File Comparator | Owned source/focus/target paths; ambiguous target basenames leave the second field empty. |
| Batch Rename | Owned full paths and per-item parents; relative mode retains the common root; Undo retains identities. |
| Checksum | Captured Calculate seeds and modeless paths; Verify uses the manifest's actual parent. |
| Split & Combine | Actual split parent; explicit selected parts retain individual paths; automatic siblings use the focused part's parent. |
| Automation | COM disk items and collections own wide paths and metadata across refresh. |
| Salamatrix | Shared sides layer supplies exact paths with the unchanged schema for all five runtime providers. |
| PictView Regenerate Thumbnail | Captured files, wide paths, temporary replacement and EXIF I/O. |
| ZIP menu / 7-Zip Test Archive | Owned selected/focused archive-file paths, including nested and Unicode parents. |
| PAK Optimize | Owned archive-file paths and actual parent notifications; requires an existing input rather than creating a missing archive. |
| Undelete Restore / Connect image | Owned per-item restore sources and exact wide image prefill; image backend limits are checked explicitly. |
| DemoPlug | Working example of capture before progress work. |

PictView navigation uses the separate wide viewer-enumeration service. Host
disk actions and Shell integration use host item resolvers. These contracts
serve different consumers; support in one command does not certify every
third-party plug-in command.

Consumers must preserve paths through their I/O. Central `SafeFile` now uses
wide Windows APIs and long-path handling for create/open/overwrite/retry.
Write retry reopens without truncating completed data and restores its offset.
`SalGetFileSize2` similarly opens UTF-8 paths through wide APIs with an ACP
fallback for legacy callers.

Legacy engines can retain explicit limits. ZIP's archive filename and
Automation's script filename still have a `SAL_MAX_PATH` **byte** capacity;
migrated callers reject over-capacity UTF-8 paths instead of truncating them.
File Comparator intake matches its existing byte capacity too. The SDK
snapshot itself preserves complete UTF-16 paths. Undelete's legacy disk-image
backend requires losslessly ACP-encodable paths within its existing buffer;
Connect image displays the exact wide path but rejects unsupported input.
Restore encrypted files uses full wide source and destination paths.

Salamatrix JSON context keeps the existing 64-selected-item limit and reports
`selectedCount` / `selectedItemsTruncated`. All five providers retain their
65,536-byte result buffer: the complete escaped JSON payload must fit in
65,535 UTF-8 bytes plus its terminator. Larger responses fail explicitly,
without partial or truncated paths. Native `ItemInfo` keeps its existing field
capacities; dynamic JSON serialization does not change that native ABI.


## Host-managed archive operations

Packing retains the existing `PackToArchive` contract: a source root and a
`SalEnumSelection2` callback with relative names. The host adapts recursive
selections centrally. `one/same.png` and `two/same.png` become distinct entries,
opened under the supplied root. ZIP and 7-Zip packing need no Branch-specific
interpretation in the plug-in.

The callback preserves order, metadata and selection across enumeration reset.
Invalid identities, indexes or conversions cancel instead of returning guessed
names. Mode 3 file-link size queries retain Retry/Ignore/Ignore All/Cancel.
Cached sizes and ignored queries survive reset, including cleanup after a move
packer has deleted sources. External packers still require their executable
and configuration and retain their own encoding and archive-format limits.

The historical Borland WinSCP source tree is outside the current native build;
its selected-only synchronization still uses a common root and basename filter.
That separate synchronization contract has not been migrated or certified by
this change. It is distinct from the current SFTP plug-in and host-managed
file-transfer callbacks.

## Validation boundaries

Native fixtures compile actual host, SDK and consumer methods. They check
lifetime after refresh, modes, metadata, duplicates, thread restrictions,
failure handling, legacy fallback and unchanged vtable prefixes. SafeFile and
consumer I/O fixtures use disposable Unicode and long-path files. Command
harnesses replace archive engines and UI dependencies; they are not GUI tests.

Earlier integration-runtime Computer Use tests packed seven Branch files with
internal ZIP and 7-Zip. Each archive retained seven exact relative names and
matching decompressed SHA-256 hashes; originals were unchanged. Coverage
included six duplicate basenames, Czech/CJK parents, a Unicode basename, a path
over 260 UTF-8 bytes and a 333-unit UTF-16 path. This is packing coverage, not
manual verification of every plug-in command. Focused suites are registered
in the normal native-test runner. Undelete's restore fixtures exercise actual
copy/read/write and cleanup with a controlled EFS callback stub. They do not
exercise Windows EFS import/export on truly encrypted files or change encryption
settings.
