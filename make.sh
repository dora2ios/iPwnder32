#!/bin/sh


./BUILD_arm64
./BUILD_x86_64

lipo -create -output iPwnder32 -arch arm64 ipwnder32_arm64 -arch x86_64 ipwnder32_x86_64
