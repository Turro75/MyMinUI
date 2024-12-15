#!/bin/sh

TARGET=$1
mkdir -p /opt/r36s-toolchain
if [ -d /opt/r36s-toolchain/usr ]; then
	rm -fr /opt/r36s-toolchain/usr
fi
cp -rf ~/buildroot/output/host/* /opt/r36s-toolchain/
# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cd /opt/r36s-toolchain
ln -s ./ usr

if [ -f ~/hwcap.h ]; then
    cp ~/hwcap.h arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/
fi

cp ~/relocate-sdk.sh /opt/r36s-toolchain/
cp ~/sdk-location /opt/r36s-toolchain/
/opt/r36s-toolchain/relocate-sdk.sh

#pack the toolchain for speed up next builds


# move the rootfs.img and tar.gz to workspace
mv ~/buildroot/output/images/rootfs.tar.gz  /opt/r36s-toolchain/
mv ~/buildroot/output/images/rootfs.ext2  /opt/r36s-toolchain/

cd /opt/
tar -czvf r36s_toolchain_glibc.tar.gz r36s-toolchain/
printf "r36s_toolchain_glibc.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"




