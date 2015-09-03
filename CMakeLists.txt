project(yextract CXX)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})


find_package(Clang REQUIRED)
include_directories(${CLANG_INCLUDE_DIRS})

SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -D__STDC_LIMIT_MACROS  -D__STDC_CONSTANT_MACROS --std=c++11") 

option(CLANG37 "CLang 3.7+" ON)

if(CLANG37)
set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DCLANG37") 
set(EXTRACLANG LLVM-3.7svn)
else()
set(EXTRACLANG LLVM-3.5)
endif()


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

add_executable(yextract yextract.cpp)
target_link_libraries(yextract ${CL_LIBS} ${CLANG_LIBS})
