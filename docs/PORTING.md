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
- Branch: `work/stability-fixes-20261115`
- Version: `0.31.1`
- Commit: `a6bb0e248c9f58867ff16dc325342aa419a7147c`

Listi is Copyright (c) 2026 Kambuzi EDB and is MIT licensed.

This pin records the Listi version used for the current parity review. It is a
reference, not a promise that Listista waits for Listi releases.

0.31.1 renames the four workspaces to Einn, Askr, Tvískipt and Smiðja. It does
not add file-management behaviour; Listista keeps the old workspace command
ids as keymap migration aliases and reads the old single-panel setting as a
fallback.

The default `haiku-native` branch still describes 0.2.0. Compare against the
working branch, not the default branch.

## Platform status

1. Linux, Qt 6: implemented and verified (Ubuntu 24.04 amd64, Qt 6.4.2).
2. FreeBSD, OpenBSD, NetBSD, macOS and Windows, Qt 6: portable source, but the
   builds have not been verified.
3. Haiku: not a Listista target. Listi is the native Haiku application.

The shared product name does not imply that every platform edition is equally
mature. State what has actually been built and checked.

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
| Transactional copy, read-back before source removal | yes | yes | Size verification before source removal in moves |
| Trash restore | yes | yes | FileOps::restoreFromTrash + emptyTrash command |
| Duplicate, symbolic link | yes | yes | file.duplicate, file.symlink commands |
| Properties, permissions, BFS attributes | yes | partial (local read/write) | Qt permissions editor; BFS attributes Haiku-only |
| Open with | yes | yes | file.openWith application chooser |
| Clipboard copy/cut/paste | yes | yes | Keep |
| Drag and drop | yes | yes | Internal and external drops handled |
| Archives: browse/read | yes | yes | Keep |
| Archives: write/edit, passwords, multipart | yes | partial (rebuild write, encrypted ZIP creation and password retention, split sets read-only) | Split `.z01/.zip` and `.001` sets are joined for browsing and extraction; editing them in place is refused instead of silently flattening the set |
| Archive extraction jobs | yes | yes | runTransfer progress + conflict policy |
| SMB/SFTP/FTP/FTPS connection UI | yes | yes | tools.connect dialog (credentials in URI/memory) |
| Remote write | partial | partial (all schemes implemented; SFTP verified end-to-end locally; explicit-TLS FTPS (ftpes://) verified end-to-end against a local server; implicit-TLS ftps:// handled by libcurl but not locally qualified; SMB untested) | Test real SMB servers; qualify implicit-TLS FTPS data channel |
| Recursive search | yes | yes | findFiles with name/ext/size/date filters |
| Folder compare | yes | yes | Recursive topmost-only compare with size checks and optional SHA-256; selection by extension still to come |
| Command palette, shortcut sheet | no | yes | Keep as a Listista convenience |
| Rebindable keys | partial | yes | Keep; default to Listi's actions |
| Config | BMessage | INI | Keep; do not share the file format. Existing `Lister` settings are migrated to the `Listista` path on first launch |
| Packaging | HPKG | CMake install + DEB/TGZ | `cmake --build build --target package` |

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
   per-item error reporting.
4. **Archive encryption.** (implemented) Compress now prompts for an optional
   password (ZIP AES-256 via libarchive) and cached passwords are reused and
   retained across archive edits/rebuilds.
5. **Remote write verification.** SFTP verified end-to-end against a local
   server (mkdir, put, list, rename, remove, recursive remove). Explicit-TLS
   FTPS (`ftpes://`, AUTH TLS on a plain FTP connection) verified end-to-end
   against a local pyftpdlib server, including uploads and downloads over the
   TLS data channel. Implicit-TLS (`ftps://`) is passed to libcurl's implicit
   mode but has not been qualified against a local server. SMB still needs a
   real server (libsmbclient).
6. **Archive write tests.** `tests/test_archive.cpp` (8 tests) exercises put,
   list, mkdir, rename, remove and password preservation across rebuilds.
7. **Archive editing.** Passwords, multipart sets and editing existing archives
   remain partial; split sets are intentionally read-only.
8. **BFS attributes.** Keep this Haiku-only except for the local permission
   editor already implemented.

## Non-goals for now

- A shared C or C++ core before both projects need the same concrete code.
- Qt on Haiku as the primary build.
- Tracker/BFS attributes in Listista.
- Plan 9 or 9front support.
- A universal provider/plugin framework.
