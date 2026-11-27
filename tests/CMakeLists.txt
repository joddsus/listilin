find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_vfsuri test_vfsuri.cpp ../src/vfs.cpp)
target_include_directories(test_vfsuri PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_vfsuri PRIVATE Qt6::Core Qt6::Test)
add_test(NAME VfsUri COMMAND test_vfsuri)

add_executable(test_foldercompare test_foldercompare.cpp ../src/foldercompare.cpp)
target_include_directories(test_foldercompare PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_foldercompare PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent)

find_path(ARCHIVE_INCLUDE_DIR2 archive.h)
find_library(ARCHIVE_LIBRARY2 NAMES archive)
add_executable(test_archive test_archive.cpp ../src/vfs.cpp ../src/vfs_archive.cpp ../src/archivepaths.cpp ../src/atomicfile.cpp)
target_include_directories(test_archive PRIVATE ${CMAKE_SOURCE_DIR}/src ${ARCHIVE_INCLUDE_DIR2})
target_link_libraries(test_archive PRIVATE Qt6::Core Qt6::Test ${ARCHIVE_LIBRARY2})
add_test(NAME Archive COMMAND test_archive)
add_test(NAME FolderCompare COMMAND test_foldercompare)

add_executable(test_renamebatch test_renamebatch.cpp ../src/renamebatch.cpp ../src/pathsafety.cpp)
target_include_directories(test_renamebatch PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_renamebatch PRIVATE Qt6::Core Qt6::Test)
add_test(NAME RenameBatch COMMAND test_renamebatch)

add_executable(test_fileops test_fileops.cpp ../src/fileops.cpp ../src/filehash.cpp ../src/moverecheck.cpp ../src/pathsafety.cpp ../src/vfs.cpp
    ../src/atomicfile.cpp ../src/transferplan.cpp ../src/treesnapshot.cpp)
target_include_directories(test_fileops PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fileops PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FileOps COMMAND test_fileops)

add_executable(test_transferplan test_transferplan.cpp ../src/transferplan.cpp)
target_include_directories(test_transferplan PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_transferplan PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TransferPlan COMMAND test_transferplan)

add_executable(test_treesnapshot test_treesnapshot.cpp ../src/treesnapshot.cpp ../src/filehash.cpp)
target_include_directories(test_treesnapshot PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_treesnapshot PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TreeSnapshot COMMAND test_treesnapshot)

add_executable(test_atomicfile test_atomicfile.cpp ../src/atomicfile.cpp)
target_include_directories(test_atomicfile PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_atomicfile PRIVATE Qt6::Core Qt6::Test)
add_test(NAME AtomicFile COMMAND test_atomicfile)

add_executable(test_archivecopy test_archivecopy.cpp ../src/fileops.cpp ../src/filehash.cpp
    ../src/moverecheck.cpp ../src/pathsafety.cpp ../src/vfs.cpp ../src/vfs_archive.cpp
    ../src/archivepaths.cpp ../src/atomicfile.cpp ../src/transferplan.cpp ../src/treesnapshot.cpp)
target_include_directories(test_archivecopy PRIVATE ${CMAKE_SOURCE_DIR}/src ${ARCHIVE_INCLUDE_DIR2})
target_link_libraries(test_archivecopy PRIVATE Qt6::Core Qt6::Test ${ARCHIVE_LIBRARY2})
add_test(NAME ArchiveCopy COMMAND test_archivecopy)

add_executable(test_upload test_upload.cpp ../src/fileops.cpp ../src/filehash.cpp
    ../src/moverecheck.cpp ../src/pathsafety.cpp ../src/vfs.cpp ../src/vfs_local.cpp
    ../src/vfs_archive.cpp
    ../src/archivepaths.cpp ../src/atomicfile.cpp ../src/transferplan.cpp ../src/treesnapshot.cpp)
target_include_directories(test_upload PRIVATE ${CMAKE_SOURCE_DIR}/src ${ARCHIVE_INCLUDE_DIR2})
target_link_libraries(test_upload PRIVATE Qt6::Core Qt6::Test ${ARCHIVE_LIBRARY2})
add_test(NAME Upload COMMAND test_upload)

add_executable(test_uploadundo test_uploadundo.cpp ../src/fileops.cpp ../src/filehash.cpp
    ../src/moverecheck.cpp ../src/pathsafety.cpp ../src/vfs.cpp ../src/vfs_local.cpp
    ../src/vfs_archive.cpp ../src/archivepaths.cpp ../src/atomicfile.cpp
    ../src/transferplan.cpp ../src/treesnapshot.cpp)
target_include_directories(test_uploadundo PRIVATE ${CMAKE_SOURCE_DIR}/src ${ARCHIVE_INCLUDE_DIR2})
target_link_libraries(test_uploadundo PRIVATE Qt6::Core Qt6::Test ${ARCHIVE_LIBRARY2})
add_test(NAME UploadUndo COMMAND test_uploadundo)

add_executable(test_searchscan test_searchscan.cpp ../src/searchscan.cpp ../src/vfs.cpp
    ../src/vfs_local.cpp ../src/vfs_archive.cpp ../src/pathsafety.cpp ../src/atomicfile.cpp
    ../src/archivepaths.cpp)
target_include_directories(test_searchscan PRIVATE ${CMAKE_SOURCE_DIR}/src ${ARCHIVE_INCLUDE_DIR2})
target_link_libraries(test_searchscan PRIVATE Qt6::Core Qt6::Test ${ARCHIVE_LIBRARY2})
add_test(NAME SearchScan COMMAND test_searchscan)

add_executable(test_searchtask test_searchtask.cpp ../src/searchscan.cpp ../src/searchtask.cpp
    ../src/vfs.cpp ../src/vfs_local.cpp ../src/pathsafety.cpp ../src/atomicfile.cpp)
target_include_directories(test_searchtask PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_searchtask PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent)
add_test(NAME SearchTask COMMAND test_searchtask)

add_executable(test_moverecheck test_moverecheck.cpp ../src/moverecheck.cpp)
target_include_directories(test_moverecheck PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_moverecheck PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MoveRecheck COMMAND test_moverecheck)

add_executable(test_settingsmigration test_settingsmigration.cpp ../src/settingsmigration.cpp)
target_include_directories(test_settingsmigration PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_settingsmigration PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SettingsMigration COMMAND test_settingsmigration)

add_executable(test_clipboardtofile test_clipboardtofile.cpp ../src/clipboardtofile.cpp)
target_include_directories(test_clipboardtofile PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_clipboardtofile PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME ClipboardToFile COMMAND test_clipboardtofile)

add_executable(test_jobreport test_jobreport.cpp ../src/jobreport.cpp)
target_include_directories(test_jobreport PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_jobreport PRIVATE Qt6::Core Qt6::Test)
add_test(NAME JobReport COMMAND test_jobreport)

add_executable(test_shelf test_shelf.cpp ../src/shelf.cpp)
target_include_directories(test_shelf PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_shelf PRIVATE Qt6::Core Qt6::Test)
add_test(NAME Shelf COMMAND test_shelf)

add_executable(test_layoutstore test_layoutstore.cpp ../src/layoutstore.cpp)
target_include_directories(test_layoutstore PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_layoutstore PRIVATE Qt6::Core Qt6::Test)
add_test(NAME LayoutStore COMMAND test_layoutstore)

add_executable(test_imagedrop test_imagedrop.cpp ../src/imagedrop.cpp)
target_include_directories(test_imagedrop PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_imagedrop PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME ImageDrop COMMAND test_imagedrop)

add_executable(test_externaltools test_externaltools.cpp ../src/externaltools.cpp)
target_include_directories(test_externaltools PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_externaltools PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ExternalTools COMMAND test_externaltools)

add_executable(test_commandregistry test_commandregistry.cpp ../src/command.cpp)
target_include_directories(test_commandregistry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_commandregistry PRIVATE Qt6::Core Qt6::Test Qt6::Gui)
add_test(NAME CommandRegistry COMMAND test_commandregistry)

add_executable(test_duplicatefinder test_duplicatefinder.cpp ../src/duplicatefinder.cpp ../src/filehash.cpp)
target_include_directories(test_duplicatefinder PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_duplicatefinder PRIVATE Qt6::Core Qt6::Test)
add_test(NAME DuplicateFinder COMMAND test_duplicatefinder)

add_executable(test_filehash test_filehash.cpp ../src/filehash.cpp)
target_include_directories(test_filehash PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_filehash PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FileHash COMMAND test_filehash)

add_executable(test_fileshred test_fileshred.cpp ../src/fileshred.cpp)
target_include_directories(test_fileshred PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fileshred PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FileShred COMMAND test_fileshred)

add_executable(test_archivepaths test_archivepaths.cpp ../src/archivepaths.cpp)
target_include_directories(test_archivepaths PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_archivepaths PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ArchivePaths COMMAND test_archivepaths)

add_executable(test_recentfolders test_recentfolders.cpp ../src/recentfolders.cpp)
target_include_directories(test_recentfolders PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_recentfolders PRIVATE Qt6::Core Qt6::Test)
add_test(NAME RecentFolders COMMAND test_recentfolders)

add_executable(test_flatscan test_flatscan.cpp ../src/flatscan.cpp)
target_include_directories(test_flatscan PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_flatscan PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FlatScan COMMAND test_flatscan)

add_executable(test_filelistexport test_filelistexport.cpp ../src/filelistexport.cpp)
target_include_directories(test_filelistexport PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_filelistexport PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FileListExport COMMAND test_filelistexport)

add_executable(test_terminallauncher test_terminallauncher.cpp ../src/terminallauncher.cpp)
target_include_directories(test_terminallauncher PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_terminallauncher PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TerminalLauncher COMMAND test_terminallauncher)

add_executable(test_remoteuri test_remoteuri.cpp ../src/remoteuri.cpp)
target_include_directories(test_remoteuri PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_remoteuri PRIVATE Qt6::Core Qt6::Test)
add_test(NAME RemoteUri COMMAND test_remoteuri)

add_executable(test_pathsafety test_pathsafety.cpp ../src/pathsafety.cpp)
target_include_directories(test_pathsafety PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_pathsafety PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PathSafety COMMAND test_pathsafety)

add_executable(test_textdiff test_textdiff.cpp ../src/textdiff.cpp)

add_executable(test_undostack test_undostack.cpp ../src/undostack.cpp)
target_include_directories(test_undostack PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_undostack PRIVATE Qt6::Core Qt6::Test)
add_test(NAME UndoStack COMMAND test_undostack)
target_include_directories(test_textdiff PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_textdiff PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TextDiff COMMAND test_textdiff)

add_executable(test_millercolumns test_millercolumns.cpp ../src/millercolumns.cpp)
target_include_directories(test_millercolumns PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_millercolumns PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MillerColumns COMMAND test_millercolumns)

add_executable(test_settingsafety test_settingsafety.cpp ../src/settingsafety.cpp)
target_include_directories(test_settingsafety PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_settingsafety PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SettingsSafety COMMAND test_settingsafety)

add_executable(test_shellsession test_shellsession.cpp ../src/shellsession.cpp)
target_include_directories(test_shellsession PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_shellsession PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ShellSession COMMAND test_shellsession)

add_executable(test_sshauth test_sshauth.cpp ../src/sshauth.cpp)
target_include_directories(test_sshauth PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sshauth PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SshAuth COMMAND test_sshauth)

add_executable(test_propertiesbatch test_propertiesbatch.cpp ../src/propertiesbatch.cpp)
target_include_directories(test_propertiesbatch PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_propertiesbatch PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PropertiesBatch COMMAND test_propertiesbatch)

add_executable(test_folderwatcher test_folderwatcher.cpp ../src/folderwatcher.cpp)
target_include_directories(test_folderwatcher PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_folderwatcher PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FolderWatcher COMMAND test_folderwatcher)
