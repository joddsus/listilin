# Listista and Listi

Listi on Haiku is the original project and keeps that identity. A Listista is
a fan of Listi. This fan-made, independently developed Qt adaptation starts
from Listi's source and interface, keeps a clearly recognisable workflow, and
then makes its own technical and product choices. Listista has its own version
numbers and release cadence.

Listista for Linux, Listista for FreeBSD, Listista for Windows and Listista for
macOS are editions of the same product. Every edition is a proud member of the
Listi fan club.

## Pinned reference

- Repository: `kambuziarendal/listi`
- Branch: `work/format-review-20261118` (release tag `v0.32.1`)
- Version: `0.32.1`
- Commit: `d383d23cf37701a9156fe00d711a24002f19f481`

Listi is Copyright (c) 2026 Kambuzi EDB and is MIT licensed.

This pin records the Listi version used for the current parity review. It is a
reference, not a promise that Listista waits for Listi releases.

0.31.1 renamed the four workspaces to Einn, Askr, Tvískipt and Smiðja without
adding file-management behaviour; Listista keeps the old workspace command ids
as keymap migration aliases and reads the old single-panel setting as a
fallback.

0.32.0/0.32.1 is a large feature and stability round on top of that: Miller
columns as a selectable pane view (default in Einn), a session Shelf for
collecting entries, clipboard text and images saved to files, configurable
workspace tools and named layouts, individual job reports with waiting and
completed jobs in Operations, unreadable preferences preserved instead of being
overwritten by defaults, archive writers for ar, ARJ, Cabinet, LHA, ISO and
RAR5 in addition to ZIP/7z/tar, compressed TAR (Unix compress), clipboard
image drops with conversion, and rechecked committed local move destinations
before source removal. Listista has not ported these yet; the parity matrix
marks them as gaps.

The default `haiku-native` branch still describes 0.2.0. Compare against the
working branch/tag, not the default branch.

## Platform status

1. Linux, Qt 6: implemented and verified (Ubuntu 24.04 amd64, Qt 6.4.2).
2. FreeBSD, OpenBSD, NetBSD, macOS and Windows, Qt 6: portable source, but the
   builds have not been verified.
3. Haiku: not a Listista target. Listi is the native Haiku application.

The shared product name does not imply that every platform edition is equally
mature. State what has actually been built and checked.

## Building against the minimum Qt version

The minimum supported Qt is 6.4. Ubuntu 24.04 (noble) ships Qt 6.4.2, which is
the version the package is built and checked against. `tools/build-qt64.sh`
makes that build reproducible on a machine without root: it downloads the
pinned noble packages, unpacks them into `~/qt64`, fixes the relocated paths,
and then configures, builds and tests `build-qt64/`.

    tools/build-qt64.sh

The first run downloads about 13 MB into `~/.cache/listista-qt64` and checks
every archive against a pinned SHA-256. Later runs reuse the cache after
re-checking it, and a corrupt archive is downloaded again. `--refresh`
re-unpacks the prefix, `--setup-only` stops after unpacking, `--no-tests` skips
the unit tests, and `--help` lists the options. `QT64_PREFIX` changes the
prefix, `QT64_CACHE` the cache, `QT64_BASE_URL` the archive mirror (the
checksums still apply), `QT64_EXTRA_PREFIX` the extra prefix for
libcurl/libarchive and `QT64_JOBS` the default job count. The script unpacks
exactly the pinned package list, so a stray `.deb` in the cache is ignored.

The packages are pinned to `6.4.2+dfsg-21.1build5`:

- Qt runtime: `libqt6core6t64`, `libqt6gui6t64`, `libqt6widgets6t64`,
  `libqt6network6t64`, `libqt6concurrent6t64`, `libqt6dbus6t64`,
  `libqt6test6t64`, `libqt6sql6t64`, `libqt6xml6t64`, `libqt6opengl6t64`,
  `libqt6openglwidgets6t64`, `libqt6printsupport6t64`
- Qt development files and tools: `qt6-base-dev`, `qt6-base-dev-tools`,
  `qt6-qpa-plugins`, `qmake6`, `qmake6-bin`
- Runtime support: `libb2-1`, `libglvnd0`, `libmd4c0`, `libopengl0`

`dpkg-deb -x` unpacks each archive under `~/qt64`, but the Qt CMake files
contain absolute `/usr/...` paths. The script rewrites `/usr/` to
`~/qt64/usr/` in the CMake, pkg-config and mkspecs files. The CMake prefix is
therefore **`$HOME/qt64/usr`**, not `$HOME/qt64`:

    cmake -S . -B build-qt64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$HOME/qt64/usr" -DLISTISTA_BUILD_TESTS=ON
    cmake --build build-qt64

The script reads the generated `build-qt64/CMakeCache.txt` and stops with an
explanation if CMake resolved `Qt6Core` from somewhere other than the private
prefix, for example because an old `build-qt64/` was configured against a
system Qt.

When libcurl and libarchive are not installed system-wide but live in `~/qt6`
(as in the Freebuff cloud workspace), append that prefix:
`-DCMAKE_PREFIX_PATH="$HOME/qt64/usr;$HOME/qt6"`. The script does this
automatically when `~/qt6/lib/libcurl.so` exists. CMake separates list entries
with `;`, not `:`; with a colon only the first prefix is searched, so OpenGL,
libcurl and libarchive from `~/qt6` go missing.

The tests and the application must load the relocated Qt libraries first:

    export LD_LIBRARY_PATH="$HOME/qt64/usr/lib/x86_64-linux-gnu:$HOME/qt6/lib:$LD_LIBRARY_PATH"
    ./build-qt64/tests/test_vfsuri
    ./build-qt64/tests/test_foldercompare
    ./build-qt64/tests/test_archive
    ./build-qt64/tests/test_renamebatch

    QT_QPA_PLATFORM=offscreen \
    QT_PLUGIN_PATH="$HOME/qt64/usr/lib/x86_64-linux-gnu/qt6/plugins" \
        ./build-qt64/listista --path /tmp --screenshot /tmp/listista-qt64.png

The `$HOME/qt6/lib` entry in `LD_LIBRARY_PATH` supplies libcurl and libarchive
in the Freebuff cloud workspace; drop it when those libraries are installed
system-wide (the loader ignores directories that do not exist).

`cmake` and `ninja` must be on `PATH`. In the Freebuff cloud workspace they
live in `~/pypkg/bin` and the pip `cmake` wrapper needs `PYTHONPATH=~/pypkg`;
the script adds both when `cmake` is not already on `PATH`.

## Development principles

- **Listi is the reference.** Port what a person can do and see, not the BeAPI
  class layout. Listi's source is the starting point; Qt idioms are the
  implementation.
- **Listista is independent.** It may make technical and product choices that
  differ from Listi. Record the reason for a divergence and keep the shared
  workflow recognisable.
- **No waiting for a release.** Listista may implement an item from Listi's
  roadmap before the Haiku edition does. If Listi later chooses another design,
  document the difference rather than silently following either side.
- **No copied UI code.** Both projects are MIT, but BeAPI views, messages and
  layout code do not translate. Reimplement the interaction with Qt widgets.
- **Preserve attribution.** Keep Listi's MIT notice and credit, and keep
  Listista's own copyright and license notices intact.
- **Platform code stays at the edge.** URI parsing, entries, sorting, filters,
  job descriptions and conflict policy should not depend on Qt widgets.
- **Safety beats parity.** If Listi verifies a move by reading the destination
  back before deleting the source, Listista must not skip that check just to
  ship the menu item earlier.
- **Mark the gaps.** A menu entry that does nothing is worse than a missing one.
  Disable it, label it, or leave it out until the behaviour exists.

## Reviewing the reference

There is no background synchronisation. On request:

1. Fetch the current Listi working branch.
2. Update the pin and the matrix below.
3. Choose the next slice using Listista's roadmap and priorities.
4. Adapt it as a vertical slice: model/operation first, then the Qt UI.
5. Build and check the slice.
6. Note what is still missing.

A Listi release is a reason to review the reference and choose the next slice,
not a reason to rewrite Listista. Listista's release notes describe its own
changes, not Listi's.

## Parity matrix

Status: **yes** = behaviour exists, **partial** = some paths work, **no** = not
adapted yet.

| Area | Listi 0.31.1 | Listista | Next step |
|---|---|---|---|
| Two panels, source/destination roles | yes | yes | Keep |
| Editable paths, back/forward, refresh | yes | yes | Keep |
| Places, saved locations | yes | yes | Bookmarks add/remove/rename; volumes and home |
| Workspaces (Einn/Askr/Tvískipt/Smiðja) | yes | yes | Four named layouts over panels, folder tree and Places; Tvískipt keeps tall panels, Smiðja adds previews |
| Workspace settings compatibility | yes | yes | Numeric and ASCII workspace ids, the legacy `ui/singlePanel` flag, and old `view.*` key bindings all load |
| Folder tree | yes | yes | Optional folder tree dock (view.toggleFolderTree) |
| Details/icons/large icons/thumbnails | yes | yes | Details, icon view; thumbnails for local files and, in the background, for archive and remote images |
| Image preview | yes | yes | Keep |
| Name filter | yes | yes | Keep |
| Copy/move/mkdir/rename/trash | yes | yes | Keep |
| Operation queue, pause/resume | yes | yes | Pause/resume wired in OperationDialog; conflict policy landed together |
| Conflict policy (Replace/Skip/All/Abort) | yes | yes | Replace/Skip/Replace All/Skip All/Abort dialog in copy/move |
| Transactional copy, read-back before source removal | yes | yes | Size verification before source removal in moves; SHA-256 read-back verification for copied files |
| Trash restore | yes | yes | FileOps::restoreFromTrash + emptyTrash command |
| Duplicate, symbolic link | yes | yes | file.duplicate, file.symlink commands |
| Properties, permissions, BFS attributes | yes | partial (local read/write) | Qt permissions editor; BFS attributes Haiku-only |
| Open with | yes | yes | file.openWith application chooser |
| Clipboard copy/cut/paste | yes | yes | Keep |
| Drag and drop | yes | yes | Internal and external drops handled |
| Archives: browse/read | yes | yes | Keep |
| Archives: write/edit, passwords, multipart | yes | partial (rebuild write, encrypted ZIP creation and password retention, split sets read-only, unsupported ARJ/CAB/LHA/RAR5 writers rejected clearly) | Split `.z01/.zip` and `.001` sets are joined for browsing and extraction; editing them in place is refused instead of silently flattening the set. ARJ/CAB/LHA/RAR5 can be browsed and extracted, but libarchive cannot write them, so creation now fails with a clear message instead of producing a mislabelled archive |
| Archive extraction jobs | yes | yes | runTransfer progress + conflict policy |
| SMB/SFTP/FTP/FTPS connection UI | yes | yes | tools.connect dialog (credentials in URI/memory) |
| Remote write | partial | partial (all schemes implemented; SFTP verified end-to-end locally; explicit-TLS FTPS (ftpes://) and implicit-TLS FTPS (ftps://) verified end-to-end against a local pyftpdlib server; SMB untested) | Remote connection inputs are now validated by RemoteUri before a URI is built; test real SMB servers |
| Recursive search | yes | yes | findFiles with name/ext/size/date filters |
| Folder compare | yes | yes | Recursive topmost-only compare with size checks and optional SHA-256; optional extension filter in the compare dialog |
| Command palette, shortcut sheet | no | yes | Keep as a Listista convenience |
| Rebindable keys | partial | yes | Keep; default to Listi's actions |
| Config | BMessage | INI | Keep; do not share the file format. Existing `Lister` settings are migrated to the `Listista` path on first launch |
| Packaging | HPKG | CMake install + DEB/TGZ | `cmake --build build --target package` |
| Miller columns pane view | yes (default in Einn) | yes (model + dock + selection wiring) | `MillerColumns::Navigator` models the column stack; `MillerColumnsView` browses folders in columns and drives the active panel on activation; toggle with `view.millerColumns` |
| Session Shelf (collect entries) | yes | yes (store, dock, navigation, delete, and whole-Shelf copy/move through the operation queue) | `file.collectToShelf`, `view.showShelf` and `view.clearShelf` operate on a session Shelf store covered by tests/test_shelf.cpp; a dock lists collected entries, Delete removes one, and double-click navigates to it. `shelf.copyToFolder` and `shelf.moveToFolder` now send the whole Shelf through the operation queue (`runTransfer`) |
| Clipboard text/image to file | yes | yes | `file.clipboardToFile` saves clipboard text or images to the active local folder with a non-clobbering name; ClipboardToFile module covered by tests/test_clipboardtofile.cpp |
| Named workspace layouts/tools | yes | yes (four named layouts, saved layouts, configurable external tools with editor dialog and menu rebuild) | `view.saveLayout` and `view.applyLayout` store named layouts (panel count, folder tree, Places, panel URIs) in QSettings; LayoutStore serialisation is covered by tests/test_layoutstore.cpp. `tools.external.*` commands are loaded from the `tools/external` settings list ("name|program|args") and launched with the active panel's folder as working directory |
| Job reports (waiting/completed) | yes | yes (waiting/running/completed/failed/cancelled reports, report dialog, and throttled Operations dock with per-job cancel, clear-finished and double-click to open reports) | JobReport::Store records waiting, running, completed, failed and cancelled jobs; `view.jobReports` lists them and `view.clearJobReports` clears finished ones. The Operations dock updates live but coalesces progress rebuilds to one refresh per 250 ms, and repaints immediately on queue/start/finish; individual waiting/running jobs can be cancelled from the dock, finished reports can be cleared, and double-clicking a row opens the full report dialog. Covered by tests/test_jobreport.cpp |
| Preserve unreadable preferences | yes | yes (migration and writes guarded) | Legacy-to-Listista migration preserves an existing but unreadable settings file; SettingsSafety validates keys/values and writes settings atomically; covered by tests/test_settingsmigration.cpp and tests/test_settingsafety.cpp |
| Archive writers (ar, ARJ, CAB, LHA, ISO, RAR5, compress TAR) | yes | partial (ZIP, 7z, tar with gzip/bzip2/xz/zstd/compress, ISO, ar, cpio) | Writable formats now include `.tar`, `.tar.gz/.bz2/.xz/.zst/.Z`, `.7z`, `.iso`, `.ar` and `.cpio` via libarchive; ARJ, CAB, LHA and RAR5 are readable but cannot be written by libarchive, and attempts to create them are rejected with a clear message. Archive entry paths and split-archive detection are covered by tests/test_archivepaths.cpp |
| Clipboard image drops with conversion | yes | yes (ImageDrop conversion, convert command, and drag-and-drop save as PNG) | ImageDrop detects and converts images (PNG, JPEG, WebP, BMP, GIF, TIFF), covered by tests/test_imagedrop.cpp; a dropped image is saved as PNG in the target folder, and `file.convertImage` converts selected images to a chosen format. Clipboard-to-file still writes PNG only |
| Move destination recheck before source removal | yes | yes (size + identity/device recheck before source removal) | MoveRecheck module snapshots and re-checks the committed destination; covered by tests/test_moverecheck.cpp |
| Icon lookup device-open avoidance | yes | n/a | Haiku-specific performance; no direct Qt equivalent |

## Possible next slices

These are candidates, not a queue dictated by Listi's release schedule. The
operation queue, trash restore, archive jobs and workspaces listed earlier are
already present.

1. **Select files of the same type.** (implemented) Context-menu action
   `selection.sameType` selects every file sharing the current file's extension.
2. **Saved search criteria.** (implemented) The Find dialog now saves and
   recalls named filter sets via QSettings.
3. **Mass rename.** (implemented) `file.renameBatch` opens a dialog that
   transforms every selected file's name with find/replace, prefix, suffix,
   optional numbering and extension preservation, with a live preview and
   per-item error reporting. The transformation lives in
   `src/renamebatch.*` and is covered by `tests/test_renamebatch.cpp`, so the
   naming rules are tested without a dialog.
4. **Archive encryption.** (implemented) Compress now prompts for an optional
   password (ZIP AES-256 via libarchive) and cached passwords are reused and
   retained across archive edits/rebuilds.
5. **Remote write verification.** SFTP verified end-to-end against a local
   server (mkdir, put, list, rename, remove, recursive remove). Both
   explicit-TLS FTPS (`ftpes://`, AUTH TLS on a plain FTP connection) and
   implicit-TLS FTPS (`ftps://`, TLS from the first byte) were verified
   end-to-end against a local pyftpdlib server, including uploads and
   downloads over the TLS data channel. SMB remains unverified: this
   environment has neither libsmbclient nor an SMB server/credentials, so no
   real-server SMB test was attempted.
6. **Archive write tests.** `tests/test_archive.cpp` (8 tests) exercises put,
   list, mkdir, rename, remove and password preservation across rebuilds.
7. **Archive editing.** Passwords, multipart sets and editing existing archives
   remain partial; split sets are intentionally read-only.
8. **BFS attributes.** Keep this Haiku-only except for the local permission
   editor already implemented.
9. **Move destination recheck.** (implemented) `MoveRecheck` snapshots and
   re-checks the committed destination (size, mtime, device and inode) before
   the source is removed; wired into `FileOps::copyAll` and `copyUris`.
10. **Preserve unreadable settings.** (implemented) Legacy-to-Listista
    migration preserves unreadable files and `SettingsSafety` validates keys and
    values and persists settings atomically; covered by tests.
11. **Clipboard to file.** (implemented) `file.clipboardToFile` saves clipboard
    text or images to the active local folder with a suggested, non-clobbering
    name; `ClipboardToFile` module covered by tests.
12. **Miller columns view.** (implemented) `MillerColumnsView` shows folders in
    columns in a dock toggled by `view.millerColumns`; activating an entry drives
    the active panel so commands operate on it.
13. **Session Shelf.** (partial) The Shelf store, collect/show/clear commands
    and a dock with navigation and Delete-to-remove exist; moving the whole
    Shelf through the operation queue remains.
14. **Named workspace layouts.** (partial) Layouts can be saved and applied;
    configurable external tools are not ported.
15. **Job reports.** (partial) Waiting/running/completed/failed/cancelled
    reports are recorded and listed; an Operations panel is not ported.
16. **Clipboard image drops.** (partial) `ImageDrop` converts images between
    supported formats and is tested; drag-and-drop wiring remains.
17. **Archive writers.** (partial) libarchive-backed writing now covers ZIP, 7z,
    tar with gzip/bzip2/xz/zstd/compress, ISO, ar and cpio. ARJ/CAB/LHA/RAR5
    are readable through libarchive but have no writer there; creation is now
    rejected with a clear message instead of producing a mislabelled archive.

## Non-goals for now

- A shared C or C++ core before both projects need the same concrete code.
- Qt on Haiku as the primary build.
- Tracker/BFS attributes in Listista.
- Plan 9 or 9front support.
- A universal provider/plugin framework.
