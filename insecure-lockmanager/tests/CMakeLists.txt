# Macro to add test files to gtest
include(test_macros)


package_add_test_with_libraries(lockmanager_test "${CMAKE_CURRENT_SOURCE_DIR}/lockmanager-t.cpp" lckMgr "${PROJECT_DIR}")
package_add_test_with_libraries(transaction_test "${CMAKE_CURRENT_SOURCE_DIR}/transaction-t.cpp" lckMgr "${PROJECT_DIR}")
package_add_test_with_libraries(lock_test "${CMAKE_CURRENT_SOURCE_DIR}/lock-t.cpp" lckMgr "${PROJECT_DIR}")
# package_add_test_with_libraries(server_test "${CMAKE_CURRENT_SOURCE_DIR}/server-t.cpp" lckMgrClient lckMgrServer "${PROJECT_DIR}")
add_executable(server_test "${CMAKE_CURRENT_SOURCE_DIR}/server-t.cpp")
target_link_libraries(server_test gtest gmock gtest_main lckMgrClient lckMgrServer)
gtest_discover_tests(server_test
        WORKING_DIRECTORY ${PROJECT_DIR}
    )
set_target_properties(server_test PROPERTIES FOLDER tests)

add_executable(hashtable_test "${CMAKE_CURRENT_SOURCE_DIR}/hashtable-t.cpp")
target_link_libraries(hashtable_test gtest gmock gtest_main hashtable lock transaction)