# File-operation safety: audit findings and fixes

This document answers the external audit of `main` at `e47e325` (27 November
2026). It lists each finding, what changed, and the regression test that proves
the new behaviour.

Verified on Linux (Ubuntu 24.04 amd64, Qt 6.4.2, libarchive 3.7.4) by building
`LISTISTA_BUILD_TESTS=ON` and running the off-screen CTest suite: **36/36
tests pass**. The GUI itself was not clicked through in this environment; the
unit tests call the same `FileOps`, archive and shred entry points that the Qt
commands use.

## Confirmed data-loss findings

### 1. Moving a file onto itself deleted the only copy

`Move here` in the folder a file already lives in copied the file onto itself
and then deleted the “source”, which was now the published copy.

- **Fix:** `TransferPlan` validates every job before the first write. An
  identity move is a no-op without an overwrite dialog; a folder cannot be
  moved or copied into itself or a subfolder; two selected items that would
  land on one destination are refused. `copyAllResult` and `copyUrisResult`
  share the check, so panel commands, paste and drag-and-drop all go through it.
- **Tests:** `TransferPlan` (same path, symlinked parent, folder into itself,
  duplicate names), `FileOps::moveToSameFolderKeepsFile`,
  `FileOps::moveFolderIntoItselfRejected`.

### 2. Symlinks and hard links could damage files outside the selection

Moving a folder symlink, emptying the trash, or securely deleting a link could
walk into or overwrite the real target.

- **Fix:** removal always classifies the entry with `lstat` semantics first
  (`TreeSnapshot::readLinkTarget`, `QFileInfo::isSymLink`) and removes the link
  itself. `removeTreeLocal` is the single removal path for moves, trash,
  restore and empty-trash. `FileShred::shredFile` removes a symlink instead of
  overwriting its target, and unlinks a file with more than one hard link
  instead of overwriting data shared with another name.
- **Tests:** `FileOps::moveSymlinkKeepsTargetContents`,
  `FileOps::emptyTrashKeepsSymlinkTarget`,
  `FileShred::shredsSymlinkNotItsTarget`,
  `FileShred::shredsFolderSymlinkNotItsTarget`,
  `FileShred::shredDoesNotOverwriteOtherHardLinks`.

### 3. A move could lose the last edit, empty files, links or metadata

The old move copied and then deleted the source after two `statPath` calls on
the same file, which compared nothing. Copying dropped `user.*` attributes,
timestamps and hard-link identity; directory moves always copy-and-delete, even
on one volume, and symlinks were stored with an absolute target.

- **Fix:** on one volume a move is a real `QFile::rename`, preserving
  everything. Cross-volume moves capture a `TreeSnapshot` of the source before
  copying, compare the destination against it (names, types, sizes, link
  targets and SHA-256 contents), compare the source again to catch edits made
  during the move, and only then remove the source. Metadata (permissions,
  access/modification times, `user.*` xattrs) is copied and verified. Relative
  symlink text is preserved exactly.
- **Tests:** `TreeSnapshot` (missing empty file, changed content, changed link
  target, missing link), `FileOps::movePreservesModeAndLinks`,
  `FileOps::moveKeepsSourceWhenItChangesDuringCopy`.

### 4. Failed extraction and archive self-collision destroyed files

Archive members were written straight to the final target, so a read error
truncated an existing file. A member named like the archive, copied into the
archive's folder with “Replace”, replaced the archive itself.

- **Fix:** archive, local, FTP/FTPS/SFTP and SMB fetches all write to a
  sibling and publish with `AtomicFile::publish`; a failure leaves the old
  target untouched. `copyUrisResult` refuses a job whose destination is a
  container archive being read, before anything is written.
- **Tests:** `TestArchive::failedExtractionKeepsExistingTarget`,
  `TestArchiveCopy::archiveMemberCannotReplaceOwnArchive`,
  `TestArchiveCopy::memberExtractsToAnotherFolder`, `AtomicFile` tests.

### 5. Editing a TAR destroyed modes and symlinks of untouched members

Only names, the folder flag, time and contents were stored, so a rewritten TAR
turned `0755` into `000` and a symlink into an empty file. The read loop also
accepted a truncated archive without checking for `ARCHIVE_EOF`.

- **Fix:** members keep file type, mode bits, symlink and hard-link targets,
  uid/gid and mtime. Short writes are errors. A read must reach `ARCHIVE_EOF`
  or the edit is refused. The rewritten archive is reopened and compared with
  what should be in it before it replaces the original, which is published
  atomically. Entry types Listista cannot write (devices, sockets, pipes) make
  the archive read-only with an explanation. Renaming onto an existing member
  is refused, and a file cannot replace a folder entry.
- **Tests:** `TestArchive::tarEditPreservesModeAndSymlink`,
  `TestArchive::renameEntry`, `TestArchive::unsafeEntryPathRejected`.

### 6. Delete could act on a neighbouring file with a trailing space

`VfsUri::parse` trimmed every URI, including URIs built from selected rows, so
`note.txt ` became `note.txt`. Clipboard paste also rebuilt local URIs through
percent-encoding.

- **Fix:** parsing no longer trims; only the address bar trims what the user
  types. Clipboard paste builds `file://` from the literal local path, and the
  cut marker lives in the clipboard data, not in a window flag.
- **Tests:** `VfsUri::trailingSpaceIsPartOfTheName`,
  `VfsUri::specialCharactersRoundTrip`, `VfsUri::unicodeRoundTrip`.

## Workflow findings from the interface review

| Finding | Fix | Test / verification |
|---|---|---|
| Sorting reordered rows without remapping the selection | Panel captures selected names and the current row, sorts, then reselects by name | `panel.cpp` sort handler; source inspection |
| Async folder change reused old row names | `DirModel::setUri` resets the model and clears rows before the scan starts | `dirmodel.cpp`; source inspection |
| Miller columns had a separate selection from the hidden view | `MillerColumnsView::selectedUris/currentName`, used by `Panel` for every command | `millercolumnsview.cpp`, `panel.cpp`; source inspection |
| Ctrl+X flag could turn an external copy into a move | Cut marker stored in the clipboard data and compared with the token this window wrote | `clipboardCopy`/`clipboardPaste`; source inspection |
| Undo entries were created before the job and could delete pre-existing data | Undo is written from the job outcome, only for destinations that did not exist, with a tree fingerprint checked again at undo time | `undoLast`, `runTransfer`; `TreeSnapshot::fingerprint` tested |
| Editor truncated the original before saving was confirmed, and a failed read opened an empty editor | Read failures keep the file closed; local saves go through `AtomicFile`; external changes are detected; upload failures keep the dialog and text open; read-only locations are shown as read-only | `AtomicFile` tests; source inspection |
| Trash failure mid-selection offered to delete the whole list; Restore used only the leaf name | `moveToTrashDetailed` reports per item and keeps undo for what succeeded; `restoreLocalPaths` matches the recorded original path | `FileOps::trashReportsPerItem`, `FileOps::restoreOnlyTheMatchingTrashEntry` |
| “Replace All” leaked from one job to the next | The conflict mode is a per-job object captured by that job's callback | `runTransfer`/`resolveConflict`; source inspection |
| A crash during replacement could leave the real name missing | Replacement uses one atomic rename of a fully written sibling; no backup name is exposed | `AtomicFile` tests |
| Search stopped only between matches and reported every stop the same way | The scan answers Cancel while walking, does not follow symlinked folders, and distinguishes limit, cancellation, completion and unreadable folders | `findFiles`; source inspection |
| Duplicate ignored copy errors and ran on the UI thread with `QFile::copy` | Uses the verified `FileOps::copyRecursively` and reports failures | `file.duplicate`; source inspection |
| Remote/archive upload had its own loop without a conflict dialog; skipped items were lost | Uploads ask the same conflict question (replace, skip, keep both, abort) and the completed job reports how many items were skipped | `runTransfer`; source inspection |

Copy and move titles now name the destination folder, and a move into an
archive or remote folder is presented as a copy because the source is kept.

## Not fixed in this change

- The search scan still runs in the window thread. It processes Cancel
  frequently and reports partial results, but a very large tree can still make
  the window less responsive until it finishes.
- Remote destinations do not verify an upload by reading it back, so a move is
  never performed there (the source is kept deliberately).
- Crash recovery cannot remove leftover hidden `.listista-*` staging files
  after a hard power loss. They contain complete or partial new copies only;
  the published name always holds one complete version.
- No interactive GUI test was run. The dialog interactions listed above were
  reviewed in source, not clicked through.

## Reproduce

    cmake -S . -B build -DLISTISTA_BUILD_TESTS=ON
    cmake --build build --parallel
    QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

On a headless runner also set `QT_PLUGIN_PATH` to the Qt platform-plugins
folder; otherwise the three widget tests abort before running.
