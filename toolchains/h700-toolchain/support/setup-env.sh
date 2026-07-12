export PATH="/opt/h700-toolchain/bin:${PATH}:/opt/h700-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/bin"
export CROSS_COMPILE=/opt/h700-toolchain/bin/arm-buildroot-linux-gnueabihf-
export PREFIX=/opt/h700-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr
export UNION_PLATFORM=h700
##export MYARCH="-marm -mtune=cortex-a35 -mfpu=neon -mfloat-abi=hard -march=armv7ve"
export MYARCH="-marm -mcpu=cortex-a35 -mfpu=neon-fp-armv8 -mfloat-abi=hard "
export LD_LIBRARY_PATH=/opt/h700-toolchain/lib

#export PATH="/opt/h700-toolchain/bin:${PATH}:/opt/h700-toolchain/aarch64-buildroot-linux-gnu/sysroot/usr/bin"
#export CROSS_COMPILE=/opt/h700-toolchain/bin/aarch64-buildroot-linux-gnu-
#export PREFIX=/opt/h700-toolchain/aarch64-buildroot-linux-gnu/sysroot/usr
#export UNION_PLATFORM=h700
#export MYARCH="-march=armv8-a+crc+simd -mtune=cortex-a35  -mcpu=cortex-a35"
#export LD_LIBRARY_PATH=/opt/h700-toolchain/lib