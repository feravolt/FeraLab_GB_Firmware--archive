___________________________________________________________________________________________________________

                     Linaro GCC 4.9 Toolchain optimized for Cortex-A8 cpu - README
___________________________________________________________________________________________________________


This repo contains latest Linaro GCC 4.9 toolchain optimized for Cortex-A8 cpu with Neon-VFPv3 technology
support (like Qualcomm Snapdragon Scorpion cpu mounted on Samsung S Plus I9001), this is suitable for
Android kernel development and includes also latest Linaro GDB.

The toolchain has been built using latest official crosstool-NG toolchain builder and is configured with
Cortex-A8 specific settings for target architecture and target optimizations:
    CT_ARCH_ARCH="armv7-a"
    CT_ARCH_CPU="cortex-a8"
    CT_ARCH_TUNE="cortex-a8"
    CT_ARCH_FPU="neon"
    CT_ARCH_FLOAT_SOFTFP=y
    CT_ARCH_FLOAT="softfp"
    CT_ARCH_ARM_MODE="arm"
    CT_ARCH_ARM_MODE_ARM=y

You can find other toolchain builds on my GitHub and also the zipped versions on my Mediafire, please take
a look at the original thread on XDA Developers forum at this link:
       http://forum.xda-developers.com/showthread.php?t=2098133
