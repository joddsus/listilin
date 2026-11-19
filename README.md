# Listista

**A proud member of the Listi fan club.**

Listista is a fan-made, independently developed adaptation of
[Listi](https://github.com/kambuziarendal/listi) for Haiku. It brings Listi's
familiar interface to other operating systems while following its own release
schedule. Every platform edition is a proud member of the Listi fan club.

Listi on Haiku is the original. A Listista is a fan of Listi, and this Qt
adaptation starts from Listi's source and interface, keeps a clearly
recognisable workflow, and then makes its own technical and product choices.
It has its own version numbers and release cadence, and it may implement ideas
from Listi's roadmap before the Haiku edition does.

This is an alpha adaptation of an alpha application. File operations can lose
data; keep backups.

## Relationship to Listi

- Listi on Haiku keeps its identity as the original.
- Listista aims for approximately the same interface and a clearly
  recognisable workflow.
- Listista is developed independently and has its own release cadence.
- Listista's version numbers and releases need not follow Listi's.
- Listista may implement roadmap items before the Haiku edition does.
- Independent choices remain compatible with preserving the kinship with
  Listi.

Listista for Linux, Listista for FreeBSD, Listista for Windows and Listista for
macOS are editions of the same product. The program name is always Listista;
the platform is a qualifier, not a different application.

See [docs/PORTING.md](docs/PORTING.md) for the pinned Listi reference, the
development principles and the parity matrix.

## Platform support

Listista is currently developed and verified on Linux. The package is built and
checked off-screen on Ubuntu 24.04 amd64 against Qt 6.4. The source uses Qt 6
and portable libraries, but the other platforms have not been verified.

| Platform | Status |
| --- | --- |
| Linux | Implemented; built, packaged and checked off-screen on Ubuntu 24.04 amd64 with Qt 6.4 |
| FreeBSD, OpenBSD and NetBSD | Portable Qt source; build not verified |
| macOS | Portable Qt source; build not verified |
| Windows | Portable Qt source; build not verified |
| Haiku | Not a Listista target; Listi is the native Haiku application |

## Building

Requirements: Qt 6 (Core, Gui, Widgets, Concurrent), libarchive, libcurl, a C++17
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
    ./build/listista

To build and test against the minimum supported Qt (6.4) on Ubuntu 24.04
without installing Qt system-wide:

    tools/build-qt64.sh

The script downloads pinned Qt 6.4.2 packages and unpacks them into `~/qt64`
(the CMake prefix is `~/qt64/usr`), then builds `build-qt64/` and runs the unit
tests. See
[docs/PORTING.md](docs/PORTING.md#building-against-the-minimum-qt-version) for
the package list and the manual commands.

To install, or to build a `.deb` (Debian/Ubuntu) alongside a `.tar.gz`:

    sudo cmake --install build --prefix /usr/local
    cmake --build build --target package

The package is produced as `listista_<version>_<arch>.deb`. Its dependencies
are listed in `CMakeLists.txt`; override them with
`-DCPACK_DEBIAN_PACKAGE_DEPENDS="..."` if your distribution uses other names.

## Running

    ./build/listista                # opens your home folder
    ./build/listista --path /tmp    # open a specific folder
    ./build/listista -p /tmp/demo

The first run shows a short introduction. `Ctrl+K` opens the command palette;
`Ctrl+Shift+K` lists every shortcut. `Help > About Listista` shows the project
identity, version and the link to Listi.

## Configuration

Settings live in `~/.config/Listista/Listista.conf` (INI). On first launch
after the rename, Listista copies an existing `~/.config/Lister/Lister.conf`
into the new location, so sessions, bookmarks and the keymap survive. The old
file is left untouched. Internal command ids such as `lister.open` keep their
old prefix for the same compatibility reason.

There you can change which buttons appear in the toolbar (`ui/buttons`), rebind
any command (`keymap/<command id>`), or restore the defaults by deleting the
file. `Ctrl+,` opens a Preferences dialog that edits the same keymap by hand.

## What works today

- Two panels with sortable columns (Name, Size, Modified), async scanning and
  an empty state when a folder has nothing in it.
- Copy, move, new folder, rename, trash (freedesktop trash, created on demand)
  and permanent delete, with a progress dialog and cancel for big operations.
  Further operations queue behind the current one instead of being refused.
- Archives (zip, tar, 7z, rar, iso, …), FTP, FTPS (implicit and explicit),
  SFTP and SMB browsed as folders; files are fetched out on demand. Split sets
  (`.z01`…`.zip`, `.001`…) are joined for reading; editing them in place is
  refused so the parts are never flattened by accident.
- Drag and drop between the panels, and dropping onto a folder row enters it.
- Recursive folder compare with topmost-only missing entries, size checks and
  optional SHA-256 content verification. It runs off the UI thread and can be
  cancelled while large trees are being read.
- “Compress to archive…” turns a selection into a ZIP, 7z, tar (gzip, bzip2,
  xz, zstd or Unix compress), ISO, ar or cpio archive in one step, optionally
  password-protected (ZIP AES-256); “Extract here” unpacks a selected archive
  into the other panel.
- “Rename selected…” renames many files at once with find/replace, prefix,
  suffix, numbering and extension preservation, with a live preview.
- Command palette, shortcut sheet, menus and toolbar all generated from one
  registry, so adding a command makes it discoverable everywhere.
- Right-click menus split into file commands and folder commands, with a
  one-click switch between them.
- Session restore, a “Places” sidebar (Home, Trash, volumes) and bookmarks
  (`Ctrl+D`) for local, archive and remote locations.
- Workspace layouts in the View menu: Einn, Askr, Tvískipt and Smiðja, each a
  named combination of panels, folder tree and Places. Einn and Askr use the
  active panel and hide the other one without discarding it. Tvískipt keeps two
  tall panels with the action bar; Smiðja adds preview panes. "Save workspace
  layout…” and “Apply workspace layout…” store your own combinations.
- A session Shelf collects entries from any panel (`file.collectToShelf`) and
  shows them in a dock; double-click navigates to an entry and Delete removes
  it. “Show or hide Shelf” and “Clear Shelf” live in the View menu.
- Job reports: waiting, running, completed, failed and cancelled transfers are
  recorded and listed by “Job reports…” in the View menu.
- Live folder filter (`Ctrl+F`), preview panes in Smiðja for text and images,
  a properties dialog (`Alt+Enter`) and “Open terminal here” (`Ctrl+T`).
- Icon view (`Ctrl+I`) with real thumbnails for local images, next to the
  details view with sortable columns.

## Known limitations

- Archives and remote locations (FTP, FTPS, SFTP) can now be written to:
  new folders, rename, delete and copy/move in. SMB has the same write code
  (mkdir, rename, delete, upload) but `libsmbclient` is optional, so whether it
  is available depends on the system.
- SMB has never been run against a real server (`libsmbclient` is not in
  conda-forge and no root was available to install it). Explicit FTPS
  (`ftpes://`, AUTH TLS) and implicit FTPS (`ftps://`, TLS from the first
  byte) were both verified end-to-end against a local pyftpdlib server:
  listing, mkdir, upload, rename, download and removal. SFTP writes (mkdir,
  rename, delete, upload, recursive delete) were verified end-to-end against a
  local test server.
- Folder compare works on local folders only. Size differences are always
  checked; content differences require the SHA-256 option and read both files.
- Clipboard works: `Ctrl+C`/`Ctrl+X`/`Ctrl+V` move files between folders and
  panels. `Alt+V` views a file and `Alt+E` edits it in the built-in editor.
- Built-in text viewer/editor and preview panes in Smiðja. Icon view shows real
  thumbnails for local images and, loaded in the background, for images inside
  archives and on FTP/SFTP/SMB shares (files up to 8 MB, two at a time).
- Tabs: one source/destination pair per tab, named after the folder it shows,
  with new/close/next/previous and “Open in new tab” in the Window menu and the
  context menus.
- Shortcuts are global: they work even when no file list has focus, but never
  steal keys from a text field.
- No interactive tests exist; everything has been verified off-screen. Expect
  to find rough edges the first time you click around.
- Wayland: build against a Qt that ships the `wayland` platform plugin, or
  run with `QT_QPA_PLATFORM=xcb` under XWayland.

## Troubleshooting

- `archive.h` or `curl/curl.h` not found: install the dev packages above.
- Black or empty window on Wayland: `QT_QPA_PLATFORM=xcb ./build/listista`.
- Want the old function-key bindings back: add `keymap/file.copy=F5` (and so
  on) to `~/.config/Listista/Listista.conf`.

## Credits

Listi is the original project and the reference for Listista:
<https://github.com/kambuziarendal/listi>. Listi is Copyright © 2026 Kambuzi
EDB and is MIT licensed. Listista is maintained by joddsus and is also MIT
licensed; see `LICENSE` for both copyright notices.
