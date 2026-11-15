cmake_minimum_required(VERSION 3.21)
project(lister VERSION 0.1.0 LANGUAGES CXX)

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
