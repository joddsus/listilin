# Capability Gap Audit: Qt Listista vs. Haiku Listi

Pinned reference: Listi `0.32.1` (`kambuziarendal/listi`, branch
`work/format-review-20261118`, commit `d383d23`). See `docs/PORTING.md`
for the full parity matrix and platform status.

## 1. Features Listi has that the Qt port lacks (or only partially has)

| Capability | Listi 0.32.1 | Qt Listista | Status |
|---|---|---|---|
| BFS attributes / Tracker metadata | yes | n/a (Haiku-only) | Non-goal for Qt |
| ARJ / CAB / LHA / RAR5 archive *writers* | yes | readable only | Blocked: libarchive has no writers |
| SMB against a real server | partial (untested) | code present, never run | Blocked: no libsmbclient / server in env |
| Archive in-place editing of split sets | yes | refused (read-only) | Intentional: prevents flattening parts |
| Properties on multiple files / recursive chmod | yes | multi-file editor with aggregate summary and recursive apply | Closed |
| Undo / redo of file operations | yes (implied) | rename, mkdir, trash, copy, local move, and uploads into an archive or server (`edit.undo`, Ctrl+Z) | Closed |
| Undo of an upload into an archive/server | yes (implied) | reads each uploaded copy back, checks the SHA-256 recorded at upload time, restores moved files and empty folders atomically, then removes the uploaded copy | Closed, tested |
| Content (full-text) search | name/ext/size/date filters | optional search inside text files, on a worker thread | Closed, tested (beyond Listi) |
| Search, checksums and duplicate finding never block the window | file scans are short and UI-driven | `SearchScan`/`SearchTask`, `FileHash` jobs and `DuplicateFinder` run on `QtConcurrent` with progress, Cancel and partial results | Closed, tested |
| Remote/archive uploads are verified, not trusted | upload reports success after the server accepts bytes | every uploaded file is read back and compared (size and SHA-256); a local source is removed only after that check | Closed, tested |
| SSH key authentication for SFTP | yes | private key, optional passphrase, known_hosts verification | Closed |
| Live local folder refresh | yes (file monitoring) | debounced watcher on the open local folder | Closed for local folders; remote/archive refresh manually |
| Icon-lookup device-open avoidance | Haiku-specific perf | n/a | Not applicable |
| Move onto itself is a no-op; no folder into itself | yes | preflight `TransferPlan` before every local job | Closed, tested |
| Symlinks/hard links never followed by delete, empty trash, shred | yes | link-safe removal and unlink-only shred | Closed, tested |
| Cross-volume move keeps source until a verified copy exists | yes | `TreeSnapshot` verification of names, types, links, contents and metadata | Closed, tested |
| Archive/remote download never truncates the target on failure | yes | staged sibling + `AtomicFile` atomic publish | Closed, tested |
| Archive edit preserves modes, symlinks and untouched members | yes | types, modes and links kept; result reopened and verified before replace | Closed, tested |
| URIs from selected rows are byte-exact (trailing spaces etc.) | yes | no trimming in `VfsUri::parse`; only the address bar trims typed text | Closed, tested |

Notes:
- Per-panel shell (Listi's optional command field) is implemented in Listista
  as `view.toggleShell`: one command line and transcript per panel, following
  that panel's local folder. It deliberately has no pseudo-terminal, so
  full-screen terminal programs are outside its scope.
- Parity on the core workflow (two panels, copy/move/mkdir/rename/trash,
  archives browse/read/write, FTP/FTPS/SFTP, SMB code, search, compare,
  workspaces, Shelf, Miller columns, job reports, named layouts, external
  tools, clipboard-to-file, image drops, move recheck) is effectively
  complete; see `docs/PORTING.md`.
- SFTP key authentication is implemented in `SshAuth` and applied to every
  libcurl SFTP handle, including saved session URIs. The private key and
  known_hosts paths are remembered; a key passphrase is never written to disk.
- Multi-file properties and recursive permission changes are implemented in
  `PropertiesBatch`, with mixed permission bits shown as partially checked.
- Local panels watch their folder through `FolderWatcher`; bursts of changes
  are coalesced into one reload. Only the displayed folder is watched, and
  remote/archive locations continue to refresh on demand.
- The external file-operation safety audit is answered in
  `docs/SAFETY_FIXES.md`, with each finding, its fix and the regression test
  that proves it. The off-screen suite now has 40 tests.
- Search runs in `SearchTask` on a worker thread, so a huge tree no longer
  freezes the window; Cancel is polled for every entry and partial results,
  the 5000-match limit and unreadable folders are reported separately. The
  optional content search reads only text files below a size limit.
- Uploads into archives, FTP/SFTP and SMB are verified by reading the file
  back and comparing SHA-256 and size. Moving a local file or folder there
  deletes the source only after that comparison; a source that already lives
  in an archive or on a server is kept deliberately and the completed job says
  so. A backend that truncates an upload now fails the job instead of losing
  data silently.
- Checksums and duplicate detection run through the same background task
  runner as copies, with progress and a working Cancel button (the duplicate
  scan polls cancellation while walking and hashing).
- Uploads are undoable. An upload records where each file and folder came
  from, where it went and the SHA-256 of every file. `edit.undo` reads the
  copies back on a worker thread, refuses the whole undo if a copy changed or
  an original name is taken again, restores moved files through a staged
  sibling and only then removes the uploaded copies. A copy whose source was
  deliberately kept is only removed after the same check. Restored files keep
  the permissions of the copy that came back; the original timestamp,
  ownership and any folder permission bits are not restored. Undo of a
  permanent delete is still not offered, because nothing is recorded for it.
- Remaining "partial" items are either Haiku-specific, blocked by missing
  libraries/servers, or deliberate safety choices (split archives stay
  read-only).

## 2. Security advantages already present in the Qt port

- `PathSafety` module: safe path components, relative-path containment,
  sanitisation, reserved Windows device names, control-char rejection.
- `PathSafety::staysWithin` / `safeJoin`: destination containment for
  untrusted remote and archive entry names (zip-slip / path-escape defense).
- Remote URI validation (`RemoteUri`): scheme/host/port/credential/path
  validation before building connection URIs; credentials redacted from
  bookmarks and saved session URIs.
- Settings safety (`SettingsSafety`): key/value validation, bounded value
  sizes, atomic writes, unreadable-file preservation, backup/restore.
- Settings migration preserves an existing but unreadable settings file.
- Transactional copy: staged temp file, read-back SHA-256 verification before
  publishing, permission/timestamp restore.
- Move destination recheck (`MoveRecheck`): size + mtime + device + inode +
  SHA-256 before source removal.
- Secure deletion (`FileShred`): multi-pass overwrite + `fsync` before unlink.
- Archive entry path cleaning and split-archive detection; unwritable formats
  (ARJ/CAB/LHA/RAR5) rejected with a clear message instead of mislabelling.
- Checksums command (SHA-256 digests to clipboard) and duplicate finder.
- Mass rename blocks unsafe result names.
- Credential redaction in bookmarks/session restore.

## 3. Functions unique to the Qt port (beyond Listi)

- Command palette (`Ctrl+K`) and shortcut sheet (`Ctrl+Shift+K`).
- Rebindable keys via INI + Preferences dialog.
- Folder compare with optional SHA-256 and extension filter.
- Duplicate-file finder and unified text diff viewer.
- Secure delete command.
- Checksums-to-clipboard command.
- External tools editor and named workspace layouts.
- Session Shelf with whole-Shelf copy/move through the operation queue.
- Job reports with throttled Operations dock, per-job cancel, clear-finished,
  double-click to open.
- Miller columns as a full panel view mode (not a dock).
- Shelf accepts context-menu sends and drag-and-drop from Listista or external apps.
- Terminal launcher detects common terminal emulators and passes the active folder safely.
- Clipboard-to-file and image-drop conversion.
- Path-safety validation in New folder / Rename.
- Keep both conflict handling in copy/move (unique destination names).
- Off-screen testable pure modules (PathSafety, RemoteUri, SettingsSafety,
  ArchivePaths, MoveRecheck, FileHash, FileShred, RenameBatch, TextDiff, ...).

## 4. Highest-impact missing capability selected and implemented

**Undo / redo of file operations** is the single highest-impact missing
capability: it is a standard expectation for a file manager, Listi exposes
reversible file operations, and the Qt port had no undo at all. Implementing
it removes the "no undo" gap and makes destructive mistakes recoverable.

Scope of this slice (reversible operations):
- Rename: undo reverts the new name to the original.
- New folder: undo removes the created (empty) folder.
- Trash: undo restores the trashed items via the existing trash-restore path.
- Copy: undo deletes the produced destination.
- Move: undo moves the destination back to the original source.

Not yet reversible in this slice: permanent delete and multi-step compound
operations; these are recorded as "not undoable" and skipped by the stack.

## 5. Implementation of this slice

- New module `src/undostack.h` / `src/undostack.cpp`: an `UndoStack` with
  `push`, `pop`, `canUndo`, `clear`, `size`, `maxSize`, and an `UndoEntry`
  describing operation type + source/destination paths. Pure, widget-free,
  unit-testable.
- `MainWindow` holds an `UndoStack` member; rename/mkdir/trash/copy/move
  commands push an entry on success.
- `edit.undo` command (`Ctrl+Z`) pops the last entry and performs the reverse
  file operation, reporting failures via status bar.
- Covered by `tests/test_undostack.cpp` (24 tests total pass).
- Documented in `README.md` and `docs/PORTING.md`.
- Built and smoke-tested off-screen; commit `d39af04`.