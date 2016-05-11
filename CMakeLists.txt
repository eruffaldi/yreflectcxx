project(yextract CXX)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})


find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(extern)
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS  -D__STDC_CONSTANT_MACROS --std=c++11") 

if(NOT ${LLVM_VERSION} VERSION_LESS "3.7")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DCLANG37PLUS") 
endif()
set(EXTRACLANG ${LLVM_LIBRARIES})
link_directories(${LLVM_LIBRARY_DIRS})


set(CL_LIBS
  clangParse
  clangFrontend
  clangSema
  clangAnalysis
  clangLex
  clangBasic
  clangSerialization
  clangDriver
  clangAST
  clangEdit
  clangTooling
  clangRewrite
  LLVMSupport
  LLVMCore
  LLVMMC
  ${EXTRACLANG}
  z
  curses
  )

add_executable(yreflect yreflect.cpp extern/jsoncpp.cpp)
target_link_libraries(yreflect ${CL_LIBS} ${CLANG_LIBS})

add_executable(yextractmp yextractmp.cpp extern/jsoncpp.cpp)
target_link_libraries(yextractmp ${CL_LIBS} ${CLANG_LIBS})
