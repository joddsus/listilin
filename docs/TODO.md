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

- [x] Add **Flat View** for local panels: recursive listing with relative paths,
  hidden-file handling and a tested `FlatScan` module. Toggle with `view.flat`.
- [x] Add **Recent folders…** per panel, derived from that panel's navigation
  history, with tested ordering/deduplication (`RecentFolders`).
- [ ] Keep comparing against upstream `kambuziarendal/listi` and close the
  remaining portable gaps (working sets and a per-panel shell are candidates).
- [x] Re-checked upstream on 2026-11-25: release HEAD is still `6195186…`
  (`v0.38.0-23`). `haiku-native` only merged that release; no new functions to
  port.
- [ ] Verify the new panel-level Miller view interactively on a desktop build;
  the current check is an off-screen build plus unit tests.
- [ ] Add a test seam for the Shelf drop path if the parser grows beyond the
  current `Shelf::entriesFromMimeData` coverage.
