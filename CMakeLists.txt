cmake_minimum_required(VERSION 3.21)
project(lister VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# Two C libraries carry the "everything is a folder" promise: libarchive reads
# every archive format worth naming, libcurl speaks FTP, FTPS and SFTP.
find_path(ARCHIVE_INCLUDE_DIR archive.h REQUIRED)
find_library(ARCHIVE_LIBRARY NAMES archive REQUIRED)
find_path(CURL_INCLUDE_DIR curl/curl.h REQUIRED)
find_library(CURL_LIBRARY NAMES curl REQUIRED)

add_executable(lister
    src/main.cpp
    src/command.cpp
    src/commandpalette.cpp
    src/shortcutsheet.cpp
    src/dirworker.cpp
    src/dirmodel.cpp
    src/fileops.cpp
    src/filetask.cpp
    src/listerdelegate.cpp
    src/listerview.cpp
    src/listerheader.cpp
    src/panel.cpp
    src/thumbnails.cpp
    src/mainwindow.cpp
    src/operationdialog.cpp
    src/vfs.cpp
    src/vfs_local.cpp
    src/vfs_archive.cpp
    src/vfs_curl.cpp
    src/vfs_smb.cpp
)

target_include_directories(lister PRIVATE src "${ARCHIVE_INCLUDE_DIR}" "${CURL_INCLUDE_DIR}")
target_link_libraries(lister PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
    "${ARCHIVE_LIBRARY}" "${CURL_LIBRARY}")

install(TARGETS lister RUNTIME DESTINATION bin)
install(FILES lister.desktop DESTINATION share/applications)

# `cmake --build build --target package` turns the install tree into a .deb on
# Debian and its derivatives, and into a .tar.gz everywhere else. No Debian
# helper scripts are involved, so the same command works on BSD and macOS.
set(CPACK_PACKAGE_NAME "lister")
set(CPACK_PACKAGE_VENDOR "Lister")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Two-panel file manager with archive and remote browsing")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "lister")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# Automatic dependency discovery needs the libraries to live in the distro's
# own paths, which is not true for a Qt installed under a private prefix. The
# list below covers the Debian/Ubuntu names (with the 64-bit time_t aliases
# used since Ubuntu 24.04); distributors can override it with
# -DCPACK_DEBIAN_PACKAGE_DEPENDS=... or turn shlibdeps back on.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6, libstdc++6, libqt6core6 (>= 6.4) | libqt6core6t64 (>= 6.4), libqt6gui6 (>= 6.4) | libqt6gui6t64 (>= 6.4), libqt6widgets6 (>= 6.4) | libqt6widgets6t64 (>= 6.4), libqt6concurrent6 (>= 6.4) | libqt6concurrent6t64 (>= 6.4), libarchive13 | libarchive13t64, libcurl4 | libcurl4t64"
    CACHE STRING "Debian dependencies for the lister package")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lister maintainers")
set(CPACK_GENERATOR "DEB;TGZ")
include(CPack)
