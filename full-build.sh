#!/bin/sh

rm -rf build
mkdir build
cd build
cmake -DEXAMPLES=OFF ..
cmake --build .
cd ..

