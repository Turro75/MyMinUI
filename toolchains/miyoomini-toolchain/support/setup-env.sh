export UNION_PLATFORM=miyoomini
export PATH="/opt/${UNION_PLATFORM}-toolchain/bin:${PATH}:/opt/${UNION_PLATFORM}-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/bin"
export CROSS_COMPILE=/opt/${UNION_PLATFORM}-toolchain/bin/arm-buildroot-linux-gnueabihf-
export PREFIX=/opt/${UNION_PLATFORM}-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr
export MYARCH="-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7-a"
export LD_LIBRARY_PATH=/opt/${UNION_PLATFORM}-toolchain/lib