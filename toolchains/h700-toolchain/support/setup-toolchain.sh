#! /bin/sh

TARGET=$1

TOOLCHAIN_GLIBC="h700_toolchain_glibc.tar.gz"
#TOOLCHAIN_GLIBC="h700_toolchain_aarch64.tar.gz"
TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/anbernic_h700(rg35xxplus)_toolchain/h700_toolchain_glibc.tar.gz"
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

mv /root/hwcap.h /opt/h700-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/


# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cp /root/relocate-sdk.sh /opt/h700-toolchain/
cp /root/sdk-location /opt/h700-toolchain/
/opt/h700-toolchain/relocate-sdk.sh
