find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_vfsuri test_vfsuri.cpp ../src/vfs.cpp)
target_include_directories(test_vfsuri PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_vfsuri PRIVATE Qt6::Core Qt6::Test)
add_test(NAME VfsUri COMMAND test_vfsuri)

add_executable(test_foldercompare test_foldercompare.cpp ../src/foldercompare.cpp)
target_include_directories(test_foldercompare PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_foldercompare PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent)
add_test(NAME FolderCompare COMMAND test_foldercompare)
