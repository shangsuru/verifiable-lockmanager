# Macro to add test files to gtest
include(test_macros)

package_add_test_with_libraries(lockmanager_test "${CMAKE_CURRENT_SOURCE_DIR}/lockmanager-t.cpp" LockManager "${PROJECT_DIR}")
package_add_test_with_libraries(transaction_test "${CMAKE_CURRENT_SOURCE_DIR}/transaction-t.cpp" LockManager "${PROJECT_DIR}")
package_add_test_with_libraries(lock_test "${CMAKE_CURRENT_SOURCE_DIR}/lock-t.cpp" LockManager "${PROJECT_DIR}")
# package_add_test_with_libraries(server_test "${CMAKE_CURRENT_SOURCE_DIR}/server-t.cpp" LockManager Client Server "${PROJECT_DIR}")