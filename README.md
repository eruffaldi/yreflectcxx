Objective
===========================

1) yreflect has the objective to extract memory layout of variables
2) yextractmp has the objective of extracing the OpenMP statements

Building
=========

Due to the various possible locations of LLVM 

cmake .. -DLLVM_INCLUDE_DIR=/opt/local/libexec/llvm-3.7/include -DLLVM_LIB_DIR=/opt/local/libexec/llvm-3.7/lib

Example of Reflector
====================

For the example test.cpp produces:

result: created 4 types
record n:Pippo q:cff::Pippo s:84 sub:
array n: q: s:20 sub:
enum n:PippoEnum q:cff::Pippo::PippoEnum s:0 sub:
record n:Pippo2 q:cff::Pippo::Pippo2 s:4 sub:

Related Tools for Reflector
===========================

- cppbinder
	https://git.acm.jhu.edu/poneil/cpp_binder/
	
- clreflection
	https://github.com/rpav/c2ffi
	Running c2ffi on this, we can get the following JSON output:
	
	Template support is limited to instantiated templates (including both classes/structs/unions and functions). c2ffi can output a new .hpp file using the -T parameter with explicit instantiations for those it finds declared but not instantiated. E.g.,

- clreflect
	https://bitbucket.org/dwilliamson/clreflect

- lldb dump
- libclang