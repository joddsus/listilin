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
| Properties on multiple files / recursive chmod | yes | single-file editor | Partial |
| Undo / redo of file operations | yes (implied) | not present | **Gap** |
| Content (full-text) search | name/ext/size/date filters | name/ext/size/date filters | **Gap** (Listi is name-based too) |
| Icon-lookup device-open avoidance | Haiku-specific perf | n/a | Not applicable |

Notes:
- Parity on the core workflow (two panels, copy/move/mkdir/rename/trash,
  archives browse/read/write, FTP/FTPS/SFTP, SMB code, search, compare,
  workspaces, Shelf, Miller columns, job reports, named layouts, external
  tools, clipboard-to-file, image drops, move recheck) is effectively
  complete; see `docs/PORTING.md`.
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
- Miller columns view dock.
- Clipboard-to-file and image-drop conversion.
- Path-safety validation in New folder / Rename.
- Off-screen testable pure modules (PathSafety, RemoteUri, SettingsSafety,
  ArchivePaths, MoveRecheck, FileHash, FileShred, RenameBatch, TextDiff, ...).

## 4. Highest-impact missing capability selected

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
- `edit.undo` command pops the last entry and performs the reverse file
  operation, reporting failures via status bar.
- Covered by `tests/test_undostack.cpp`.
- Documented in `README.md` and `docs/PORTING.md`.
EOF