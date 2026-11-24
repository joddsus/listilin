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
add_executable(test_archive test_archive.cpp ../src/vfs.cpp ../src/vfs_archive.cpp)
target_include_directories(test_archive PRIVATE ${CMAKE_SOURCE_DIR}/src ${ARCHIVE_INCLUDE_DIR2})
target_link_libraries(test_archive PRIVATE Qt6::Core Qt6::Test ${ARCHIVE_LIBRARY2})
add_test(NAME Archive COMMAND test_archive)
add_test(NAME FolderCompare COMMAND test_foldercompare)

add_executable(test_renamebatch test_renamebatch.cpp ../src/renamebatch.cpp)
target_include_directories(test_renamebatch PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_renamebatch PRIVATE Qt6::Core Qt6::Test)
add_test(NAME RenameBatch COMMAND test_renamebatch)

add_executable(test_fileops test_fileops.cpp ../src/fileops.cpp ../src/moverecheck.cpp ../src/vfs.cpp)
target_include_directories(test_fileops PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fileops PRIVATE Qt6::Core Qt6::Test)
add_test(NAME FileOps COMMAND test_fileops)

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

add_executable(test_millercolumns test_millercolumns.cpp ../src/millercolumns.cpp)
target_include_directories(test_millercolumns PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_millercolumns PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MillerColumns COMMAND test_millercolumns)
