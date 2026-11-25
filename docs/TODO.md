# Listista TODO

Last updated: 2026-11-25.

## Requested fixes

- [x] Add **Send to Shelf** to the file context menu.
- [x] Accept files dragged from a lister or another application onto the Shelf.
- [x] Hide the Places column by default in the Tvískipt workspace.
- [x] Make **Open terminal here** find an installed terminal and pass the
  active folder using that terminal's supported working-directory option.
- [x] Make Miller columns a real panel view instead of a small dock: the active
  pane switches between Details and a full-width column browser, with the path
  bar, filter and status line kept in place.

## Continues

- [ ] Keep comparing against upstream `kambuziarendal/listi` and close portable
  capability gaps (Flat View, recent folders, working sets and per-panel shell
  remain candidates).
- [ ] Verify the new panel-level Miller view interactively on a desktop build;
  the current check is an off-screen build plus unit tests.
- [ ] Add a test seam for the Shelf drop path if the parser grows beyond the
  current `Shelf::entriesFromMimeData` coverage.
