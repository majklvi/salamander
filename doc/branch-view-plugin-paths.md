# Branch View and plug-in paths

## The shared contract

A panel path identifies the location being browsed. In Branch View it is the
scan root, while each file can have a different parent. `CFileData.Name` and
`NameW` remain basenames. Combining `GetPanelPath()` with either field is not
a valid way to locate a Branch View file.

The host owns the mapping from each current row to its complete path. Disk
plug-in commands obtain it through the optional `Salamander.PanelItemPaths`
service, using the existing `QueryService` method:

```cpp
CSalamanderServiceQuery query = {SALAMANDER_SERVICE_PANEL_ITEM_PATHS,
                                SALAMANDER_PANEL_ITEM_PATHS_VERSION_1_0, 0};
CSalamanderServiceResult result = {};
if (general->QueryService(&query, &result))
{
    // Validate result.Interface and result.Version, then use
    // CSalamanderPanelItemPathsAbstract::GetItemFullPath(panel, item, ...).
}
```

Resolve every item on the UI thread while its panel pointer is still current,
then copy the resolved path into operation-owned storage before opening a
modeless dialog or starting a worker. An advertised resolver is authoritative:
a failed lookup must not fall back to a synthesized root/basename path or a
partial selection. The output is complete UTF-16; file APIs must preserve it,
including adding an extended prefix for long paths where required. A plug-in
may use an ordinary-folder fallback only when an older host does not provide
the service.

The host validates the pointer against its current contiguous row arrays
before dereferencing it. This takes constant time per lookup, including large
selections. An active Branch file without its metadata yields an empty path;
it cannot acquire the identity of an unrelated root-level file. The synthetic
`..` navigation row retains its normal behavior.

This is a generic per-item API, used in ordinary disk panels as well as Branch
View. Plug-ins do not need to detect Branch View or interpret its private
metadata. Existing SDK vtables, `CFileData` layout, names and extensions remain
unchanged. Silently replacing a basename with a path would violate the old SDK
contract and break filename masks, rename rules and binary plug-ins.

## Host-managed archive operations

Packing uses a different, existing contract: `PackToArchive` receives a source
root and a `SalEnumSelection2` callback returning relative names. The host
adapts Branch View to this contract centrally. For example, the two files
`one/same.png` and `two/same.png` are distinct archive entries, and the packer
opens each under the supplied source root.

The callback preserves the chosen file order and metadata and restarts that
same selection after an enumeration reset. Missing identities, invalid indexes
and allocation/conversion failures cancel enumeration instead of returning a
guessed path or a success-shaped partial selection. In mode 3, a file symlink
has its target's size, using the same Retry/Ignore/Ignore All/Cancel handling
as ordinary directory packing. The ignore-all decision survives an enumeration
reset within the operation.

Resolved link sizes, including an ignored query, also survive reset. This is
necessary for move packers that enumerate once more to remove directories
after their source files have already been deleted; that pass must not reopen
the removed links or show a new file-size error.

The shared `SalGetFileSize2` helper opens UTF-8 paths through wide Windows APIs,
with a validated ACP fallback for older callers. Long unprefixed paths are
made absolute on the heap before receiving an extended prefix. Already
extended paths are retained. Failure clears the size and reports the error;
allocation failure follows the same non-throwing error contract. This helper
also serves other host and plug-in operations, not only Branch View.

An external packer still needs its executable and configuration. Neither this
adapter nor the item-path service can remove encoding or archive-format limits
inside an older binary packer.

## Consumer audit

These are separate entry points. Support in one command does not certify every
command in the same plug-in. The audit below records source findings, not GUI
verification of the pending consumers.

| Consumer | Path boundary | Status |
| --- | --- | --- |
| Host disk actions, viewer/editor launch and shell selection | Host per-item resolvers | Adapted in Branch View implementation; see its action tests. |
| File Comparator | Optional item-path service | Adapted; per-item paths copied before comparison. |
| Batch Rename | Optional item-path service | Adapted; owned selection and wide filesystem operations. |
| PictView viewer navigation | Optional wide viewer-enumeration service | Adapted; distinct from thumbnail menu commands. |
| Host-managed ZIP/7-Zip packing | Source root plus relative-name callback | Adapted centrally; GUI and content audit described below. |
| Checksum Calculate/Verify | `GetPanelPath` plus seed/focused basename | Needs consumer migration. |
| Split & Combine commands | One source directory plus item basename | Needs consumer migration. |
| Automation item objects | Panel path plus basename | Needs consumer migration. |
| Salamatrix item information | Shared sides layer joins panel path and basename | Needs migration in the shared layer, preserving parity across all runtimes. |
| PictView Regenerate Thumbnail | Its own panel enumeration | Needs consumer migration; normal viewer support is independent. |
| ZIP menu commands / 7-Zip Test Archive | Their own focused/selected-item path assembly | Needs consumer migration; host-managed packing is independent. |

Updated consumers should all use the same host service. Legacy binaries that
assemble paths themselves do not automatically adopt it. The host has no
per-command capability declaration in the current SDK; this change does not
claim that every installed plug-in command is Branch-compatible, nor introduce
a blanket block of unrelated commands. Future capability negotiation must
distinguish commands consuming item paths from commands using only the panel
root or no panel items, and check both menu state and actual execution.

## Verification

In the isolated all-PR x64 build, Computer Use packed the same seven disposable
files through **ZIP (Plugin)** and **7-Zip (Plugin)**. Both archives contain the
exact seven relative names and all seven uncompressed SHA-256 hashes match the
source manifest. Source paths and bytes remained unchanged. The selection
includes six identical basenames under different parents, Czech/CJK directory
components, a Unicode basename, an absolute path of 287 UTF-8 bytes but fewer
than 260 UTF-16 units, and a true 333-unit UTF-16 path. Tests did not enable
delete-after-packing or exercise external packer executables.

Native fixtures compile the real host resolver, panel getters, archive callback
and file-size helper extracted from production sources. Dependencies are
mocked only at the surrounding panel/UI boundary; the file-size fixture uses
real disposable files and Windows handles. Tests cover ordinary-panel behavior,
exact identities, enumeration reset, symlink-size requests, cancellation,
Unicode/long paths, error reporting, and the resolver's linear total work for
large selections. Previous-revision runs demonstrate failures at the corrected
boundaries. The normal native-test runner includes these fixtures.
