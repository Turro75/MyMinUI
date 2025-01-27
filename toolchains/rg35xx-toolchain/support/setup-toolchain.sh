#! /bin/sh

TOOLCHAIN_GLIBC="rg35xx-toolchain.tar.gz"
#TOOLCHAIN_GLIBC="r36s_toolchain_aarch64.tar.gz"

TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/rg35xx_toolchain/rg35xx_toolchain.tar.gz"
#TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/sjgam_m21_toolchain/m21_toolchain_glibc.tar.gz"
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

mv /root/hwcap.h /opt/rg35xx-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/


# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cp /root/relocate-sdk.sh /opt/rg35xx-toolchain/
cp /root/sdk-location /opt/rg35xx-toolchain/
/opt/rg35xx-toolchain/relocate-sdk.sh
