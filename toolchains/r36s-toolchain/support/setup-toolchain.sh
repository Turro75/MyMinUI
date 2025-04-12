#! /bin/sh

TARGET=$1

TOOLCHAIN_GLIBC="r36s_toolchain_glibc.tar.gz"
#TOOLCHAIN_GLIBC="r36s_toolchain_aarch64.tar.gz"

TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/r36s_rg353_toolchain/r36s_toolchain_glibc.tar.gz"
TOOLCHAIN_TAR=$TOOLCHAIN_GLIBC

if [ -f "./$TOOLCHAIN_TAR" ]; then
	echo "extracting local toolchain"
else
	wget "$TOOLCHAIN_URL" -O ./$TOOLCHAIN_TAR
	echo "extracting remote toolchain "
fi
cp "./$TOOLCHAIN_TAR" /opt
cd /opt
tar xf "./$TOOLCHAIN_TAR"
rm -rf "./$TOOLCHAIN_TAR"
rm -rf "/root/${TOOLCHAIN_GLIBC}"

mv /root/hwcap.h /opt/r36s-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/


# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cp /root/relocate-sdk.sh /opt/r36s-toolchain/
cp /root/sdk-location /opt/r36s-toolchain/
/opt/r36s-toolchain/relocate-sdk.sh
