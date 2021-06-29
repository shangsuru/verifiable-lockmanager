# Macro to add test files to gtest
include(test_macros)

package_add_test(lockmanager_test "${CMAKE_CURRENT_SOURCE_DIR}/lockmanager-t.cpp")
package_add_test(transaction_test "${CMAKE_CURRENT_SOURCE_DIR}/transaction-t.cpp")