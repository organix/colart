#!/bin/bash
clang -emit-llvm tart.c -c -o tart.bc
clang -emit-llvm colart.c -c -o colart.bc
llvm-link-3.4 tart.bc colart.bc -o bootstrap.bc
llc-3.4 bootstrap.bc -o bootstrap.s
gcc bootstrap.s -o bootstrap.native
