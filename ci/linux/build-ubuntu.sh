#!/bin/sh
set -ex

mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DGLOBAL_INSTALLATION=ON .. 
make -j4
