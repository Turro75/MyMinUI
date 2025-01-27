export PATH="/opt/rg35xx-toolchain/bin:${PATH}:/opt/rg35xx-toolchain/arm-buildroot-linux-gnueabihf/sysroot/bin"
export CROSS_COMPILE=/opt/rg35xx-toolchain/bin/arm-buildroot-linux-gnueabihf-
export PREFIX=/opt/rg35xx-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr
export UNION_PLATFORM=rg35xx
export LD_LIBRARY_PATH=/opt/rg35xx-toolchain/lib
export MYARCH="-marm -mtune=cortex-a9 -mfpu=neon-fp16 -mfloat-abi=hard -march=armv7-a"