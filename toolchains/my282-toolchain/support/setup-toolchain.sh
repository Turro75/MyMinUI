#! /bin/sh


TOOLCHAIN_TAR="my282_toolchain.tar.gz"

#TOOLCHAIN_URL="https://github.com/steward-fu/website/releases/download/miyoo-a30/a30_toolchain-v1.0.tar.gz"
TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/miyoo_a30_toolchain/my282_toolchain.tar.gz"
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
rm -rf "/root/${TOOLCHAIN_TAR}"
#if [ -d /opt/a30 ]; then
#mv /opt/a30 /opt/my282-toolchain
#fi
cp /root/hwcap.h /opt/my282-toolchain/usr/arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/
cp /root/SDL_image.h /opt/my282-toolchain/usr/arm-buildroot-linux-gnueabihf/sysroot/usr/include/SDL/
cp /root/SDL_ttf.h /opt/my282-toolchain/usr/arm-buildroot-linux-gnueabihf/sysroot/usr/include/SDL/

# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cp /root/relocate-sdk.sh /opt/my282-toolchain/
cp /root/sdk-location /opt/my282-toolchain/
/opt/my282-toolchain/relocate-sdk.sh

