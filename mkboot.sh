#!/bin/bash
rm -f ../../../build-bootimg/zImage
cp arch/arm/boot/zImage ../../../build-bootimg/zImage
pushd ./
cd ../../../build-bootimg/
./makeit.sh
popd

