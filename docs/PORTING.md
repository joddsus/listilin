# Qt frontend and Listi

Listi on Haiku is the upstream project and the primary product. This repository
is the Qt frontend for Linux, the BSDs, and (best effort) macOS and Windows.
It follows Listi's behaviour; it does not define a separate feature set.

## Pinned upstream

- Repository: `kambuziarendal/listi`
- Branch: `work/stability-fixes-20261115`
- Version: `0.31.1`
- Commit: `a6bb0e248c9f58867ff16dc325342aa419a7147c`

0.31.1 renames the four workspaces to Einn, Askr, Tvískipt and Smiðja. It does
not add file-management behaviour; the Qt port keeps the old workspace command
ids as keymap migration aliases and reads the old single-panel setting as a
fallback.

The default `haiku-native` branch still describes 0.2.0. Compare against the
working branch, not the default branch.

## Platform order

1. Haiku, native BeAPI (upstream, always first)
2. Linux and the BSDs, Qt 6
3. macOS and Windows, Qt 6, best effort

A feature is not delayed on Haiku to make the Qt port easier. If a behaviour
cannot be shared, the Haiku implementation wins and the Qt frontend adapts.

## Porting rules

- **Behaviour first.** Port what a person can do and see, not the BeAPI class
  layout. Listi's source is the specification; Qt idioms are the implementation.
- **No Qt-only features.** New user-visible behaviour is added to Listi first,
  or at least agreed as a portable feature, before it appears here.
- **No copied UI code.** Both projects are MIT, but BeAPI views, messages and
  layout code do not translate. Reimplement the interaction with Qt widgets.
- **Platform code stays at the edge.** URI parsing, entries, sorting, filters,
  job descriptions and conflict policy should not depend on Qt widgets.
- **Safety beats parity.** If Listi verifies a move by reading the destination
  back before deleting the source, the Qt port must not skip that check just to
  ship the menu item earlier.
- **Mark the gaps.** A menu entry that does nothing is worse than a missing one.
  Disable it, label it, or leave it out until the behaviour exists.

## Syncing with upstream

There is no background synchronisation. On request:

1. Fetch the current Listi working branch.
2. Update the pin and the matrix below.
3. Pick the highest-priority gap that fits the session.
4. Port it as a vertical slice: model/operation first, then the Qt UI.
5. Build and check the slice.
6. Note what is still missing.

A Listi release is not a reason to rewrite the Qt frontend. It is a reason to
update the matrix and choose the next slice.

## Parity matrix

Status: **yes** = behaviour exists, **partial** = some paths work, **no** = not
ported yet.

| Area | Listi 0.31.1 | Qt frontend | Next step |
|---|---|---|---|
| Two panels, source/destination roles | yes | yes | Keep |
| Editable paths, back/forward, refresh | yes | yes | Keep |
| Places, saved locations | yes | yes | Bookmarks add/remove/rename; volumes and home |
| Workspaces (Einn/Askr/Tvískipt/Smiðja) | yes | yes | Four named layouts over panels, folder tree and Places; Einn/Askr show the active panel and hide the other without discarding it |
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
| Archives: write/edit, passwords, multipart | yes | partial (rebuild write, passwords, split sets read-only) | Split `.z01/.zip` and `.001` sets are joined for browsing and extraction; editing them in place is refused instead of silently flattening the set |
| Archive extraction jobs | yes | yes | runTransfer progress + conflict policy |
| SMB/SFTP/FTP/FTPS connection UI | yes | yes | tools.connect dialog (credentials in URI/memory) |
| Remote write | partial | partial (all schemes implemented; SFTP verified locally, FTPS/SMB untested) | Test real FTPS/SMB servers |
| Recursive search | yes | yes | findFiles with name/ext/size/date filters |
| Folder compare | yes | yes | tools.compare dialog |
| Command palette, shortcut sheet | no | yes | Keep as a Qt convenience |
| Rebindable keys | partial | yes | Keep; default to Listi's actions |
| Config | BMessage | INI | Keep; do not share the file format |
| Packaging | HPKG | CMake install + DEB/TGZ | `cmake --build build --target package` |

## First vertical slices

1. **Operation queue and conflicts.** This is the largest safety gap. Listi
   pauses, resumes, asks about conflicts and keeps the main window alive.
2. **Trash restore.** The freedesktop trash already exists; restore is the
   missing half.
3. **Archive jobs.** Create/extract with progress, conflict policy and no
   replacement of existing targets.
4. **Search parity.** Saved criteria and date/size/extension filters.
5. **Workspaces.** Einn and Tvískipt first; Smiðja only after the folder tree
exists.

## Non-goals for now

- A shared C or C++ core before both frontends need the same concrete code.
- Qt on Haiku as the primary build.
- Tracker/BFS attributes in the Qt frontend.
- Plan 9 or 9front support.
- A universal provider/plugin framework.
