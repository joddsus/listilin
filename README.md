# Lister

Lister is a two-panel file manager in the spirit of Directory Opus Magellan II,
built for Linux in 2026: fast, light on resources, Wayland-friendly, and
surprisingly pleasant to use. Everything is a command — keys, buttons, menus,
the context menu and the command palette are all just triggers for the same
command registry.

Archives, FTP, FTPS, SFTP and SMB show up as ordinary folders: you navigate
into a `.zip` the way you navigate into a directory, and copy files in and out
of it. Nothing is special-cased in the UI; the location is just a URI.

## Building

Requirements: Qt 6 (Core, Gui, Widgets), libarchive, libcurl, a C++17
compiler, CMake 3.21+ and Ninja.

Debian / Ubuntu:

    sudo apt install build-essential cmake ninja-build \
        qt6-base-dev libarchive-dev libcurl4-openssl-dev

Arch Linux:

    sudo pacman -S base-devel cmake ninja qt6-base libarchive curl

Fedora:

    sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel \
        libarchive-devel libcurl-devel

Then:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
    ./build/lister

## Running

    ./build/lister                # opens your home folder
    ./build/lister --path /tmp    # open a specific folder
    ./build/lister -p /tmp/demo

The first run shows a short introduction. `Ctrl+K` opens the command palette;
`Ctrl+Shift+K` lists every shortcut.

## Configuration

Settings live in `~/.config/Lister/Lister.conf` (INI). There you can change
which buttons appear in the toolbar (`ui/buttons`), rebind any command
(`keymap/<command id>`), or restore the defaults by deleting the file.
`Ctrl+,` opens a Preferences dialog that edits the same keymap by hand.

## What works today

- Two panels with sortable columns (Name, Size, Modified), async scanning and
  an empty state when a folder has nothing in it.
- Copy, move, new folder, rename, trash (freedesktop trash, created on demand)
  and permanent delete, with a progress dialog and cancel for big operations.
- Archives (zip, tar, 7z, rar, iso, …), FTP, FTPS, SFTP and SMB browsed as
  folders; files are fetched out on demand.
- Drag and drop between the panels, and dropping onto a folder row enters it.
- “Compress to archive…” turns a selection into a zip (or tar) in one step;
  “Extract here” unpacks a selected archive into the other panel.
- Command palette, shortcut sheet, menus and toolbar all generated from one
  registry, so adding a command makes it discoverable everywhere.
- Right-click menus split into file commands and folder commands, with a
  one-click switch between them.
- Session restore, a “Places” sidebar (Home, Trash, volumes) and bookmarks
  (`Ctrl+D`) for local, archive and remote locations.
- Live folder filter (`Ctrl+F`), a preview pane for text and images, a
  properties dialog (`Alt+Enter`) and “Open terminal here” (`Ctrl+T`).

## Known limitations

- Archives and remote locations (FTP, FTPS, SFTP) can now be written to:
  new folders, rename, delete and copy/move in. SMB remains read-only pending a
  real server to test against.
- SMB has never been run against a real server (`libsmbclient` is not in
  conda-forge and no root was available to install it). SFTP and FTPS are
  untested against real servers.
- Clipboard works: `Ctrl+C`/`Ctrl+X`/`Ctrl+V` move files between folders and
  panels. `Alt+V` views a file and `Alt+E` edits it in the built-in editor.
- Built-in text viewer/editor and preview pane; real thumbnails and tabs are
  still missing.
- Shortcuts are global: they work even when a lister does not have focus,
  but never steal keys from a text field.
- No interactive tests exist; everything has been verified off-screen. Expect
  to find rough edges the first time you click around.
- Wayland: build against a Qt that ships the `wayland` platform plugin, or
  run with `QT_QPA_PLATFORM=xcb` under XWayland.
- No git history: the whole project is uncommitted in the working tree.

## Troubleshooting

- `archive.h` or `curl/curl.h` not found: install the dev packages above.
- Black or empty window on Wayland: `QT_QPA_PLATFORM=xcb ./build/lister`.
- Want the old function-key bindings back: add `keymap/file.copy=F5` (and so
  on) to `~/.config/Lister/Lister.conf`.
