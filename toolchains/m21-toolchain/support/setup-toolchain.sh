#! /bin/sh

TARGET=$1
TOOLCHAIN_MUSL="m21_toolchain_musl.tar.gz"
TOOLCHAIN_GLIBC="m21_toolchain_glibc.tar.gz"

if [ "$TARGET" = "MUSL" ]; then
    TOOLCHAIN_TAR=$TOOLCHAIN_MUSL
    TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/sjgam_m21_toolchain/m21_toolchain_musl.tar.gz"
else
    TOOLCHAIN_TAR=$TOOLCHAIN_GLIBC
    TOOLCHAIN_URL="https://github.com/Turro75/MyMinUI_Toolchains/releases/download/sjgam_m21_toolchain/m21_toolchain_glibc.tar.gz"
fi
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
rm -rf "/root/${TOOLCHAIN_MUSL}"
rm -rf "/root/${TOOLCHAIN_GLIBC}"

if [ "$TARGET" = "MUSL" ]; then
    mv /root/hwcap.h /opt/m21-toolchain/arm-buildroot-linux-musleabihf/sysroot/usr/include/asm/
else
    mv /root/hwcap.h /opt/m21-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/
fi

# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cp /root/relocate-sdk.sh /opt/m21-toolchain/
cp /root/sdk-location /opt/m21-toolchain/
/opt/m21-toolchain/relocate-sdk.sh

if [ "$TARGET" = "MUSL" ]; then
    sed -i '1s/^/export MY_SYS=musl\n/' ~/setup-env.sh
else
    sed -i '1s/^/export MY_SYS=gnu\n/' ~/setup-env.sh
fi
