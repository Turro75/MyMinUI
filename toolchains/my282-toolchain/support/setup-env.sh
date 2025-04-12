export PATH="/opt/my282-toolchain/bin:${PATH}:/opt/my282-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/bin"
export CROSS_COMPILE=/opt/my282-toolchain/bin/arm-buildroot-linux-gnueabihf-
export PREFIX=/opt/my282-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr
export UNION_PLATFORM=my282
export MYARCH="-marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
export LD_LIBRARY_PATH=/opt/my282-toolchain/lib