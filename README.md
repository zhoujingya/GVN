## How to test

opt -load-pass-plugin=./build/lib/libGVN.dylib -passes="demo-gvn" -disable-output test.ll -S
