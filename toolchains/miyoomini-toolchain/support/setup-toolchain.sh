#! /bin/sh

TOOLCHAIN_GLIBC="miyoomini_toolchain_glibc.tar.gz"
TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/miyoomini_toolchain/miyoomini_toolchain_glibc.tar.gz"

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


mv /root/hwcap.h /opt/miyoomini-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/

# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cp /root/relocate-sdk.sh /opt/miyoomini-toolchain/
cp /root/sdk-location /opt/miyoomini-toolchain/
/opt/miyoomini-toolchain/relocate-sdk.sh

