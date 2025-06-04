#!/bin/sh

cp -a ~/lib/* ~/buildroot/output/host/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/

mkdir -p /opt/my282-toolchain
if [ -d /opt/my282-toolchain/usr ]; then
	rm -fr /opt/my282-toolchain/usr
fi
cp -rf ~/buildroot/output/host/* /opt/my282-toolchain/
# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cd /opt/my282-toolchain
ln -s ./ usr

if [ -f ~/hwcap.h ]; then
	cp ~/hwcap.h arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/
fi


cp ~/relocate-sdk.sh /opt/my282-toolchain/
cp ~/sdk-location /opt/my282-toolchain/
/opt/my282-toolchain/relocate-sdk.sh

#pack the toolchain for speed up next builds


# move the rootfs.img and tar.gz to workspace
mv ~/buildroot/output/images/rootfs.tar.gz  /opt/my282-toolchain/
mv ~/buildroot/output/images/rootfs.ext2  /opt/my282-toolchain/

cd /opt/
tar -czvf my282_toolchain_glibc.tar.gz my282-toolchain/
printf "my282_toolchain_glibc.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"




