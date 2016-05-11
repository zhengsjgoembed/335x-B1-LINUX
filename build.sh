#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

make distclean
make cm335x_tisdk_defconfig
make uImage -j4
