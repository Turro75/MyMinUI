export PATH="/opt/miyoomini-toolchain/bin:${PATH}:/opt/miyoomini-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/bin"
export CROSS_COMPILE=/opt/miyoomini-toolchain/bin/arm-buildroot-linux-gnueabihf-
export PREFIX=/opt/miyoomini-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr
export UNION_PLATFORM=miyoomini
export MYARCH="-marm -mtune=cortex-a7 -mfpu=neon -mfloat-abi=hard -march=armv7ve"
export LD_LIBRARY_PATH=/opt/miyoomini-toolchain/lib