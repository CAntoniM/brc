#!/bin/bash

mkdir build;

cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . && cd ..

if [ ! -f measurements.txt ]; then
    java -cp build/generator.jar dev.morling.onebrc.CreateMeasurements 1000000000
fi

time ./build/brc measurements.txt 
