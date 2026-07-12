#!/bin/sh

mkdir -p /opt/h700-toolchain
if [ -d /opt/h700-toolchain/usr ]; then
	rm -fr /opt/h700-toolchain/usr
fi
cp -rf ~/buildroot/output/host/* /opt/h700-toolchain/
# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cd /opt/h700-toolchain
ln -s ./ usr

if [ -f ~/hwcap.h ]; then
    cp ~/hwcap.h arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/
fi

cp ~/relocate-sdk.sh /opt/h700-toolchain/
cp ~/sdk-location /opt/h700-toolchain/
/opt/h700-toolchain/relocate-sdk.sh

#pack the toolchain for speed up next builds


# move the rootfs.img and tar.gz to workspace
mv ~/buildroot/output/images/rootfs.tar.gz  /opt/h700-toolchain/
mv ~/buildroot/output/images/rootfs.ext2  /opt/h700-toolchain/

cd /opt/
tar -czvf h700_toolchain_glibc.tar.gz h700-toolchain/
printf "h700_toolchain_glibc.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"




