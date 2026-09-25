# Branch View

Branch View presents the files below a disk folder as one panel listing. It is
implemented in the host's disk panel, so a file retains its actual location for
viewing, editing, selection and file operations. It does not create a virtual
folder or move any files.

## Using the view

- Open a local folder, mapped network drive or UNC folder and press **Ctrl+B**,
  or choose **Branch View** in the panel menu. The feature is unavailable in
  archive and plug-in filesystem panels.
- The panel keeps its current layout, including Icons, Thumbnails and custom
  Detailed templates. In Detailed view, **Path** shows each file's actual parent
  folder and supports sorting and resizing; its width belongs to this tab.
  Path is not added to Icons or Thumbnails. Existing custom columns and the
  Separate Extension setting remain available.
- Only files appear, apart from the ordinary `..` navigation row where
  applicable. Two files with the same name remain separate rows, distinguished
  by Path in Detailed view. The first scan adds results progressively while the
  panel remains usable. Later scans keep existing results visible until their
  replacement is ready.
- The directory line distinguishes initial scanning from background refresh,
  and reports completion or folders that could not be read. A background
  refresh or its cancellation reports the retained snapshot count. The tab caption identifies Branch View.
- **Stop Branch View Scan**, or Escape when quick search is not active, stops
  enumeration and keeps the currently visible results. It discards unpublished
  results and pending refresh requests, then pauses automatic refresh until an
  explicit Refresh. Filesystem changes below the root normally request a
  background refresh; they do not interrupt a scan already in progress.
- **Open Containing Folder** opens the focused file's real parent and focuses
  that file. Navigating elsewhere leaves Branch View. Ctrl+B returns to the
  ordinary folder listing and restores its sort settings. It preserves the
  current layout, including a layout selected while Branch View was active.
- F3, editing, Enter, rename, copy/move, deletion, clipboard and shell actions
  resolve the chosen row's actual path. Viewer next/previous navigation uses
  the panel's ordering, subject to the viewer's normal masks/association rules.
- F5/F6 initially copy or move selected files into the destination as a flat
  selection. The optional **Keep subfolder paths relative to the Branch View
  root** checkbox preserves their relative directory structure. Ordinary
  conflict handling still applies when destination names collide.
- Focus and selected files survive refresh by exact full path. Internal
  Save/Restore Selection also uses full paths when saved from Branch View.
  Clipboard name lists keep their existing name-only semantics.
- Duplicating a tab, reopening a closed tab, or navigating back through path
  history restores Branch View and its in-memory focus/selection snapshot.
  Restoration is asynchronous: a selected file is restored when its scan
  result arrives. Files that disappeared are not recreated.

## Scope and boundaries

The first version enumerates normal disk folders and network folders. Archives
are listed as files; the scanner never enumerates their contents. Entering an
archive invokes the existing archive view. Plug-in filesystems are outside this
version's recursive listing scope.

Directory symbolic links and junctions (name-surrogate reparse points) are
skipped. This prevents cycles and traversal outside the selected tree. Other
reparse directories, such as ordinary cloud-provider directories, are not
blanket-excluded. File links remain file entries. Hidden/system folder exclusion
follows the panel setting; file filters are applied to the collected files and
do not prune ordinary subfolders based on filename masks.

The displayed list is a changing filesystem snapshot, not a transaction.
Enumeration continues past inaccessible folders and reports their count. A
cancel request stops further work cooperatively; Windows/network drivers may
finish an outstanding SMB call later. Closing a tab does not wait for that call
or leave the worker with a pointer to the closed panel.

The scan and panel metadata preserve wide Unicode names and extended Win32
paths. A destination application or older plug-in can still impose its own
path/encoding limitations. The existing narrow `CFileData.Name` field remains a
bounded, UTF-8-safe display mirror; code needing identity must use the panel's
wide per-item resolver, not reconstruct `root + Name`.

Saved navigation history persists the Branch flag, Path width and wide focused
path. The complete selection and view snapshot are session state. This feature
does not promise restoration of the active Branch listing and every selection
after application restart.

## Host architecture

`src/branch_view.h` and `.cpp` contain the scanner, per-panel metadata and mode
transitions. No field is added to the plug-in-shared `CFileData` structure, and
`PluginData` is not repurposed for disk paths.

Each scan result owns `WIN32_FIND_DATAW`, its actual parent, and its path relative
to the scan root. The panel maps the stable allocation pointer `CFileData.Name`
to this metadata. Shallow row sorting moves the record but preserves this key.
Full wide paths are selection/focus identities; full UTF-8 paths identify icon,
thumbnail and Explorer-property cache entries. A second cache-key-to-row index
is rebuilt after sorting. Equal basenames therefore do not share cache entries.
Rename remaps the old key to the renamed record and requests a fresh scan.

Consumers use `GetItemFullPathW`, `GetItemDirectoryW`, `GetItemIdentityW`, or
`GetItemRelativePathW`. This boundary is essential: the panel's own `GetPathW()`
continues to identify the Branch root and is not the parent of every row.
Viewer enumeration passes complete paths and validates the source tab and
current item, including after reorder. PictView consumes the new wide viewer
enumeration service; existing plug-in interface slots remain in place.

The worker uses wide `FindFirstFileExW`/`FindNextFileW` and an explicit stack of
directories. It publishes groups of up to 256 files and also publishes at
folder boundaries. It owns a reference-counted scan state and no window or
panel pointer. Replacing a scan replaces that state, so a late old result
cannot enter the new listing. Cancellation sets the cooperative flag and calls
`CancelSynchronousIo`; it never forcibly terminates the scan thread.

The UI polls at 200 ms. The first scan publishes bounded previews (up to
2,048, then 8,192 entries), followed by the complete result. The progress count
continues to report all discovered files. This avoids repeatedly rebuilding a
large growing listing on the UI thread. When previous results exist, a refresh instead collects its new
snapshot in the background and replaces the visible rows atomically after
completion. Automatic refresh does not republish identical snapshots;
enumeration order and last-access timestamps alone do not count as a change.
An explicit Refresh does rebuild the view even when the filesystem snapshot is
unchanged, so filters, hidden-name settings and properties are reapplied.
Starting an automatic refresh does not invalidate the current snapshot
generation or its pending property jobs.
Publication respects the existing operation/refresh suspension guards and
reuses disk `ReadDirectory` row construction for filtering, icons and metadata.
Applying results does not synchronously reopen the root folder.

Automatic notifications never restart an active scan. After a completed scan,
the next automatic scan waits for a cooldown equal to the last scan's duration,
clamped to 5-30 seconds. Changes are also debounced for one second after the
latest notification, with a five-second maximum from the first pending
notification; the cooldown still takes precedence. An explicit Refresh bypasses
these automatic delays once no scan is active. Stop clears pending work and
pauses automatic refresh until an explicit Refresh. The directory watcher uses
subtree notifications in Branch mode and ordinary single-directory notifications
otherwise.

Before replacing rows, the icon reader is suspended. Old metadata remains
available until the old listing is released, and is destroyed on panel teardown
only after the reader stops. Destruction cancels the worker directly and never
uses the live Stop Scan action: `WM_DESTROY` has already deleted the directory
line and other child controls by the time the panel destructor runs. Custom
Path sorting also suspends the reader and
restores focus by stable item identity. Visible custom Explorer columns load
in the background even when Path determines the row order. Display-only jobs
copy at most 128 missing visible rows per batch and merge their results into the
cache. They only invalidate the affected panel, without recalculating the
layout of every row. Explicit property sorting still processes the full list. A job that only loads
column values does not sort the panel when it completes; an active Explorer
property sort retains its separate sorting behavior. Property jobs capture full
item paths and the Branch generation, so equal basenames cannot share values
and completion from an earlier snapshot is discarded. A property job cannot
start while replacement rows are being applied. Publication explicitly starts
the next job after the new rows are installed, preventing an old row collection
from being captured under the new generation. Changing the requested property
sort supersedes the previous job without falling back to synchronous shell
property reads. Panel teardown disconnects completion delivery and releases
its job reference; it does not wait for a blocked property provider.

Activating a Branch panel resumes icon work by signal and handles deferred real
filesystem notifications through the automatic scheduler. It does not probe
the root or free space synchronously and does not start a scan merely because
the application regained focus. Auxiliary icon, thumbnail and property loading
does not display the directory-line scan indicator; the text reports the actual
scan state, including completion and Stop.

## Validation

The following scopes are deliberately separate: the native test executes the
real scanner, source contracts guard host integration boundaries, and GUI tests
exercise the built application. Passing one does not imply the others passed.

| Layer | Verified scope |
| --- | --- |
| Native `branch_view_tests` | PASS: 605 files, zero failures; nested trees, duplicate basenames, Czech/CJK/emoji components, a 254-UTF-16-unit filename, UTF-8 paths over 260 bytes, full paths over 260 UTF-16 units, hidden-folder filtering, batching, replacement scan, cancellation, immediate scanner-owner destruction, UNC prefix construction, component-aware root checks, and a real directory-link cycle skipped. |
| `branch_view_contract_tests.py` | Lifecycle ordering, operation-suspend guards, coalesced asynchronous refresh, exact selection identity, recursive watcher registration, mode restoration, history ownership, duplicate/closed tabs, Path column layout, saved selection, and stale Explorer-property results. |
| Native action tests | PASS: viewer enumeration 11 checks, operation/shell/recycle 25, worker paths 9, file launching 12, copy/ADS paths 11. Launch tests use real processes; recycle tests use only disposable fixtures. |
| `branch_operations_contract_tests.py` | Separate action-boundary checks for viewing/editing, rename, operation sources, relative destinations, archive enumeration, recycling, clipboard and icon identities. |
| Custom Explorer properties | Native `branch_properties_tests`: 17 checks PASS; `branch_properties_contract_tests.py`: 14 checks PASS. Coverage includes exact item paths/cache identities, loading visible custom columns independently of Path sorting, and guarding the row-publication/property-job boundary. |
| GUI smoke verified during development | Ctrl+B produced eight files from four folders, including Unicode and a full path over `MAX_PATH`; Path text was elided for display; F3 opened the long-path file with correct full title/content and Space moved to `root.txt`; PictView displayed three different `same.png` images across nested Unicode folders with Space/Backspace and correct captions; thumbnails showed all three distinct colors. Rename changed only the intended duplicate, Path sorting and Ctrl+F9 retained its full-path focus, and a duplicated tab restored its view and focused file. The final build copied all eight files with preserved relative paths and matching SHA-256 hashes, including the Win32 long path; closing/reopening the duplicate tab, Open Containing Folder/history return, and application shutdown all passed after the teardown fix. |

Additional GUI checks passed for layout, properties and refresh behavior:

- Starting Branch View from Icons retained Icons. Changing to Thumbnails with
  Alt+5 while in Branch View, then leaving with Ctrl+B, retained Thumbnails.
- A custom Detailed template retained its Dimensions column. Three separate
  `same.png` files displayed their actual dimensions, `17x23`, `41x29` and
  `7x11`; Path sorting and an explicit Refresh preserved correct property values.
- Scanning `C:\Windows` completed with 174,935 files and 50 unreadable folders
  reported. Subsequent background scans retained the existing listing. Escape
  stopped scanning, and the stopped state persisted for more than 30 seconds.
- In the four-file fixture, applying `added*` showed the one matching item;
  removing the filter restored all four. This checked that an explicit refresh
  reapplies filtering even when the filesystem snapshot is unchanged.

A later automatic new-file test exposed a race in which a property job captured
three old rows with the new snapshot generation. The publication guard and
explicit job start described above now have seven passing source contracts.
The final Release x64 GUI retest passed: adding a PNG under a Unicode parent
updated the Path-sorted listing from four to five files and displayed its
correct Dimensions value (41 x 29) automatically, without manual Refresh.

PictView navigation was tested with `*.png` associated with PictView in the
isolated test profile. The remaining GUI cases below are a regression checklist,
not a claim of completed verification. Real SMB latency/disconnection is not simulated by the native local-filesystem test.

Run the focused source contracts without creating bytecode caches:

```powershell
python -B src/tests/branch_view_contract_tests.py
python -B src/tests/branch_operations_contract_tests.py
python -B src/tests/branch_properties_contract_tests.py
```

`src/tests/branch_view_tests.vcxproj` builds the native scanner test. The standard
`tools/run_native_tests.ps1` runner discovers native test projects and registers
the source-contract suites. The scanner test owns and removes its temporary
fixture tree; it does not modify user files.

## Responsiveness follow-up

The follow-up separates scan status from auxiliary property/thumbnail loading,
avoids activation-triggered disk probes, limits display-property snapshots to
the visible viewport, and removes redundant listing sorts and basename merges
from Branch publication. Branch identity remains the full path; selection,
cut markers and overlays survive publication by that identity.

Shell association discovery is prepared in timer slices (8 ms target, at most
64 extensions per slice, 15 ms between pending slices). The budget is checked
between queries: a single slow Shell provider cannot be interrupted. Only query
answers are cached during preparation; live association/icon indices are left
untouched until row publication. Association reload invalidates the answers and
restarts preparation. Shared cancellation and panel-lifetime guards handle
reentrant Shell callbacks, Stop, leaving Branch View and closing its tab.
The previous listing remains usable, and a separate localized preparation status
reports both the incoming count and the retained result count. Completion and
automatic refresh scheduling occur only after publication or an unchanged-snapshot
comparison. Stop remains in effect until an explicit Refresh.

Native `explorer_property_work_tests` passes 17 checks covering bounded viewport
batches, superseded sort requests, cancellation without waiting for a blocked
provider, and 100 completion-versus-closed-window races. Native preparation tests
pass 17 checks for budgets, resumption and cancellation/restart during callbacks.
The actual array reserve tests pass 20 checks each in Debug and Release, including
allocation failure, capacity rounding, insert/delete and empty-array cleanup.
Property contracts pass 14 checks; activation/preparation contracts pass 9 checks.
The native scanner still passes 605-file coverage and an added 175,000-item
Unicode/long-path comparison confirms equivalent ordering after the optimization.

On the development VM, an instrumented Release x64 scan of `C:\Windows` returned
174,935 files and 50 unreadable folders. The measured final UI publication fell
from 11,812 ms to 1,531 ms (about 87% shorter). Intermediate previews took 31 ms
and 62 ms. The 326 nonempty association-preparation slices took at most 16 ms
(the clock has coarse resolution); the previous synchronous association stage
alone took about 2.1 seconds. These are UI-stage timings, not total traversal
times or a guarantee for other machines/providers. A residual final publication
pause of about 1.5 seconds remains. Temporary profiling was removed from the
user test build; raw logs and matching binary/source manifests are retained in
the external `builds/branch-view/logs` development artifacts.

GUI verification covered navigating the opposite panel during scanning and
preparation, correct completed status, and cancelling preparation with Escape
on a disposable 1,600-extension fixture. The stopped state survived leaving and
returning to the application. A selected Unicode-parent duplicate retained its
selection, focus and Dimensions value after explicit Refresh and property sorting.

## Cancelled operation selection

Operation dialogs temporarily select the focused item when no explicit selection
exists. In Branch View, that temporary selection must be remembered by its full
wide path, not by a basename shared by files in other directories. Cancelling a
delete, copy/move or attribute dialog clears only that exact temporary item;
pre-existing explicit selections remain unchanged. If rows reorder, the identity
still finds the original item. If it disappears, cleanup must not deselect another
same-named file. Ordinary directory/archive/plugin panels retain their existing
name-matching behavior.

The follow-up was reproduced in the previous GUI build on the second `same.png`.
The fixed Release x64 build passed cancellation on all three duplicates (Escape
and No), preserved an explicit two-file selection, and cleaned up temporary
selection after cancelling F5, F6 and F2. All eight fixture files retained their
SHA-256 hashes. Native `panel_temporary_selection_tests` passes 21 checks,
including replaced/reordered rows, a missing target, exact Unicode identity and
long paths; Branch integration contracts now pass 19 checks.

## Focused regression checklist

- Select one of two identical basenames, sort by Path both ways, refresh, save
  and restore selection, and rename it. Check focus, selection, icon, thumbnail
  and Explorer property still refer to that exact parent/file.
- Cancel a large scan; refresh; navigate away; close its tab while pending.
  Reopen/duplicate the tab and use history. No late result may enter another
  listing, and restoring selection must wait for the matching item.
- Create/delete files repeatedly in a nested directory, including while an
  operation suspends refresh. Verify that an existing listing remains visible,
  changes arrive together after a scan, and bursts respect debounce/cooldown
  without restarting active scans. Stop, change a file, and confirm that only
  an explicit Refresh resumes automatic updates.
- Copy/move duplicates with flat and preserved-directory destinations, including
  collisions, skip/cancel and long Unicode paths. Test recycle/delete and a
  multi-parent shell/clipboard selection against disposable fixtures.
- Open/view/edit duplicate names; test next/previous in Internal Viewer and
  PictView with realistic masks, selected-only navigation, and after reorder.
  Verify title, content and containing directory together.
- Enter and leave Branch View from Icons, Thumbnails and custom Detailed
  layouts; also change the layout while Branch View is active. Verify that the
  chosen layout survives leaving and Path appears only in Detailed view.
- Sort by Path with custom Explorer columns visible. Wait for property values
  and confirm that their arrival does not reorder rows. Repeat after refresh
  with duplicate basenames in different folders.
- Check Separate Extension on/off, narrow Path columns, horizontal scrolling,
  tab restoration, DPI, and the application's Windows Dark Mode scheme.
- Check mapped and UNC roots, access-denied children and a disconnected share;
  distinguish an incomplete scan from an empty complete tree.
- Leave Branch View and verify ordinary directory, archive and plug-in panels,
  retained layout/restored sort, and an older binary plug-in still behave normally.
