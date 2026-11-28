# Listista TODO

Last updated: 2026-11-27.

## Requested fixes

- [x] Add **Send to Shelf** to the file context menu.
- [x] Accept files dragged from a lister or another application onto the Shelf.
- [x] Hide the Places column by default in the Tvískipt workspace.
- [x] Make **Open terminal here** find an installed terminal and pass the
  active folder using that terminal's supported working-directory option.
- [x] Make Miller columns a real panel view instead of a small dock: the active
  pane switches between Details and a full-width column browser, with the path
  bar, filter and status line kept in place.

## File-operation safety audit (27 November 2026)

- [x] Refuse moves onto themselves, folders into themselves and two sources
  with one destination (`TransferPlan`).
- [x] Never follow a symlink when moving, deleting, emptying the trash or
  shredding; shred a hard-linked file by unlinking it.
- [x] Preserve modes, timestamps, `user.*` xattrs and relative link text; use a
  real rename on one volume and a verified tree copy across volumes.
- [x] Stage archive, local and network downloads and publish atomically; block
  a member from replacing the archive it is read from.
- [x] Keep archive member types, modes and symlinks; verify the rewritten
  archive before replacing the original; refuse an edit that cannot preserve
  an entry.
- [x] Stop trimming URIs from selected rows (trailing-space file names).
- [x] Remap selection after sorting; clear rows immediately on folder change;
  use the Miller column selection for commands.
- [x] Tie cut/paste to the actual clipboard data; write undo only from a
  successful outcome and fingerprint the destination.
- [x] Keep the editor open on read/save/upload failure and detect external
  changes; atomic local saves.
- [x] Report trash failures per file and restore by recorded original path.
- [x] Per-job conflict mode; atomic replacement without a missing-name window;
  cancellable search with truthful result status; remote upload conflicts.
- [x] Document all of it in `docs/SAFETY_FIXES.md`; 36 off-screen tests passed at that point (39 now).

## Continues

- [x] Add **Flat View** for local panels: recursive listing with relative paths,
  hidden-file handling and a tested `FlatScan` module. Toggle with `view.flat`.
- [x] Add **Recent folders…** per panel, derived from that panel's navigation
  history, with tested ordering/deduplication (`RecentFolders`).
- [x] Add a per-panel shell dock: `view.toggleShell` keeps one command line and
  transcript per panel, follows the panel's local folder, and floats as a dock.
  `ShellSession` tracks a literal `cd` and passes a `cd` that needs shell
  expansion (variables, globs, `~user`) to the shell; `ShellWidget` adds the
  transcript, history and Stop. Tested in `tests/test_shellsession.cpp`.
  Divergence: it is a command field rather than a PTY terminal, so full-screen
  programs are not supported.
- [x] Add SFTP key authentication: private key, optional passphrase and
  known_hosts are set in Connect to server, validated by `SshAuth`, persisted
  as paths, and applied to every SFTP handle. Passphrases stay in memory.
- [x] Make Properties work on a whole selection with recursive permissions,
  mixed bits as partially checked, and an aggregate summary
  (`PropertiesBatch`, `tests/test_propertiesbatch.cpp`).
- [x] Refresh local folders automatically when they change on disk, with a
  debounced watcher (`FolderWatcher`, `tests/test_folderwatcher.cpp`).
- [x] Run search on a worker thread with a working Cancel and optional
  content search (`SearchScan`, `SearchTask`, `tests/test_searchscan.cpp`,
  `tests/test_searchtask.cpp`); checksums and the duplicate finder use the
  same background task runner.
- [x] Verify remote/archive uploads by reading each file back and comparing
  size and SHA-256 before removing a local source (`FileOps::uploadUrisResult`,
  `tests/test_upload.cpp`), and say clearly when a source inside an archive or
  on a server is kept.
- [x] Make settings updates preserve unrelated keys (Shelf collections, URI
  icon-view choices) instead of dropping everything the strict reader rejects,
  and give layout names collision-free keys with their display name stored
  separately (`tests/test_settingsafety.cpp`, `tests/test_layoutstore.cpp`).
- [x] Detect non-UTF-8 text before comparing it, offer Latin-1, bound the diff
  matrix before allocation and check file size before reading
  (`TextDiff::decodeText`, `TextDiff::compare`, `tests/test_textdiff.cpp`).
- [x] Activate a panel on a plain click or focus change and remember the active
  side per tab, so toolbar commands no longer use the previous panel.
- [x] Keep credentials out of shared text: the address bar, CSV export, saved
  Shelf collections and saved layouts now redact a server password, while the
  active session keeps it for the connection (`VfsUri::redactCredentials`).
- [x] Apply only the permission boxes the user actually changed, so a recursive
  change no longer copies the selected folder's mode onto private children
  (`PropertiesBatch::changesFromStates`, `tests/test_propertiesbatch.cpp`).
- [x] Report an unreadable subtree in folder comparison as incomplete instead of
  matching, and compare symlinks by their relative target text
  (`tests/test_foldercompare.cpp`).
- [x] Give every external "Open" its own temporary folder, so two archive
  members with the same name no longer overwrite each other
  (`TemporaryCopy`, `tests/test_temporarycopy.cpp`).
- [x] Keep `!` in remote paths and only treat it as an archive separator for
  the archive scheme (`tests/test_vfsuri.cpp`).
- [x] Start the export dialog on "Selected entries" when a selection exists.
- [x] Make cross-location moves two-way: moving archive/remote entries to a
  local folder now removes the original after the copy lands, reports a
  refused delete without losing either copy, verifies the download against the
  backend's listed size and digest (or a read-back hash) before deleting the
  original, and supports undo by uploading
  the copy back with read-back verification after a fingerprint check
  (`FileOps::undoMovesOutResult`, `tests/test_moveout.cpp`). Remote-to-remote
  moves still keep the source and are presented as copies.
- [x] Search inside archives and remote folders: `SearchScan::runVfs` walks
  the VFS with the existing filters, `SearchTask::startVfs` keeps it off the UI
  thread with Cancel, results navigate back to their parent, unreadable
  locations are reported, and the local-only content search is disabled and
  reported instead of silently ignored (`tests/test_searchscan.cpp`).
- [x] Make uploads undoable: record source, destination and SHA-256 for every
  uploaded file and folder, then restore moved files and empty folders after
  reading the copies back and verifying them, removing the uploaded copies
  last. A changed copy or an occupied original name stops the undo with the
  copies intact (`FileOps::undoUploadsResult`, `tests/test_uploadundo.cpp`).
- [x] Fix uploading from an archive or server, whose temporary file was
  removed before the upload read it (found by the undo work).
- [x] Keep real directory entries in rewritten archives, including empty
  folders, and make zip directory entries readable back as folders.
- [ ] Keep comparing against upstream `kambuziarendal/listi` and close the
  remaining portable gaps. Working sets (reopening a set of top-level windows)
  remain outside the current scope because Listista has tabs, not multiple
  windows. The remaining documented gaps are blocked by the environment (SMB
  has no libsmbclient/server) or deliberate (split archives stay read-only).
- [x] Re-checked upstream on 2026-11-25: release HEAD is still `6195186…`
  (`v0.38.0-23`). `haiku-native` only merged that release; no new functions to
  port.
- [ ] Verify the new panel-level Miller view interactively on a desktop build;
  the current check is an off-screen build plus unit tests.
- [ ] Add a test seam for the Shelf drop path if the parser grows beyond the
  current `Shelf::entriesFromMimeData` coverage.
