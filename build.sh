#!/bin/bash
mkdir build
pushd build
rm -rf *
c++ -Wall -pedantic -g ../src/buienradar.cpp -o buienradar \
    `sdl2-config --cflags --libs` \
    -l curl \
    `GraphicsMagickWand-config --cppflags --ldflags --libs` \
    -Wno-c99-extensions
popd
