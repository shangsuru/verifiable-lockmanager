 # to set doxygen options, see: https://www.doxygen.nl/manual/config.html
 set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
 set(DOXYGEN_GENERATE_LATEX NO)
 set(DOXYGEN_RECURSIVE YES)
 # set(DOXYGEN_WARN_AS_ERROR YES)
 set(DOXYGEN_WARNINGS YES)
 set(DOXYGEN_WARN_NO_PARAMDOC YES)
 set(DOXYGEN_WARN_IF_DOC_ERROR YES)
 set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
 set(DOXYGEN_GENERATE_TREEVIEW YES)
 set(DOXYGEN_GENERATE_HTML YES)
 set(DOXYGEN_HTML_EXTRA_STYLESHEET "${CMAKE_SOURCE_DIR}/docs/doxygen-awesome.css")
 set(DOXYGEN_GENERATE_MAN NO)
 set(DOXYGEN_HAVE_DOT YES)
 set(DOXYGEN_CALL_GRAPH NO)
 set(DOXYGEN_CALLER_GRAPH NO)
 set(DOXYGEN_FULL_PATH_NAMES NO)
 set(DOXYGEN_EXCLUDE_PATTERNS 
     "${CMAKE_SOURCE_DIR}/cmake/*" 
     "${CMAKE_SOURCE_DIR}/ethereum/adapter/include/adapter_ethereum/json.hpp"
     "${CMAKE_SOURCE_DIR}/fabric/adapter/*libclient.hpp"
     )
     
 doxygen_add_docs(
   docs # target name
 
   # folders and files that should be considered by doxygen
   ${PROJECT_SOURCE_DIR}/src
   ${PROJECT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/mainpage.md
   COMMENT "Generate documentation using doxygen"
 )