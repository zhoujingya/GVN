## How to test

opt -load-pass-plugin=./build/lib/libGVN.dylib -passes='default<O0>,function(demo-gvn)' -disable-output test.ll -S


## Reference

* https://www.cs.cmu.edu/afs/cs/academic/class/15745-s19/www/lectures/L3-Local-Opts.pdf

* https://github.com/adava/LLVM-Value-Numbering
