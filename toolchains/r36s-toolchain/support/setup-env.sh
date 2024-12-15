export PATH="/opt/r36s-toolchain/bin:${PATH}:/opt/r36s-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/bin"
export CROSS_COMPILE=/opt/r36s-toolchain/bin/arm-buildroot-linux-gnueabihf-
export PREFIX=/opt/r36s-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr
export UNION_PLATFORM=r36s
##export MYARCH="-marm -mtune=cortex-a35 -mfpu=neon -mfloat-abi=hard -march=armv7ve"
export MYARCH="-marm -mcpu=cortex-a35 -mfpu=neon-fp-armv8 -mfloat-abi=hard "
export LD_LIBRARY_PATH=/opt/r36s-toolchain/lib

#export PATH="/opt/r36s-toolchain/bin:${PATH}:/opt/r36s-toolchain/aarch64-buildroot-linux-gnu/sysroot/usr/bin"
#export CROSS_COMPILE=/opt/r36s-toolchain/bin/aarch64-buildroot-linux-gnu-
#export PREFIX=/opt/r36s-toolchain/aarch64-buildroot-linux-gnu/sysroot/usr
#export UNION_PLATFORM=r36s
#export MYARCH="-march=armv8-a+crc+simd -mtune=cortex-a35  -mcpu=cortex-a35"
#export LD_LIBRARY_PATH=/opt/r36s-toolchain/lib