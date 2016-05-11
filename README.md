Objective
===========================

1) yreflect has the objective to extract memory layout of variables
2) yextractmp has the objective of extracing the OpenMP statements

Building
=========

Tested clangs: 3.5 3.7 3.9 under OSX (for OpenMP clang 3.7+ is needed)

	cmake .. -DLLVM_CONFIG=/opt/local/bin/llvm-config-mp-3.5
	cmake .. -DLLVM_CONFIG=/opt/local/bin/llvm-config-mp-3.7 
	cmake .. -DLLVM_CONFIG=/opt/local/bin/llvm-config-mp-3.9 

Mac Port Users
==============

port installed | grep clang
port search clang | grep clang-
port install clang-3.9

Example of Reflector
====================

For the example test.cpp produces:

result: created 4 types
record n:Pippo q:cff::Pippo s:84 sub:
array n: q: s:20 sub:
enum n:PippoEnum q:cff::Pippo::PippoEnum s:0 sub:
record n:Pippo2 q:cff::Pippo::Pippo2 s:4 sub:

Related Publication
===================
The following pubblication is associated to the work discussed here. In particular the SOMA framework was aimed at static multicore partioning of the execution of multicore algorithms.

	Ruffaldi E., Dabisias G., Brizzi F. & Buttazzo G. (2016). SOMA: An OpenMP Toolchain For Multicore Partitioning. In 31st ACM/SIGAPP Symposium on Applied Computing . ACM.


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