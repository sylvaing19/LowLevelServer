#!/bin/sh

dirname="cmake-build-cli"
mkdir -p $dirname || exit
cd $dirname || exit
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -j 2 ..
make
