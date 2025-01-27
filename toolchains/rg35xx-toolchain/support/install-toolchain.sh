#!/bin/sh

mkdir -p /opt/rg35xx-toolchain
if [ -d /opt/rg35xx-toolchain/usr ]; then
	rm -fr /opt/rg35xx-toolchain/usr
fi
cp -rf ~/buildroot/output/host/* /opt/rg35xx-toolchain/
# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cd /opt/rg35xx-toolchain
ln -s ./ usr

if [ -f ~/hwcap.h ]; then
    cp ~/hwcap.h arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/
fi

cp ~/relocate-sdk.sh /opt/rg35xx-toolchain/
cp ~/sdk-location /opt/rg35xx-toolchain/
/opt/rg35xx-toolchain/relocate-sdk.sh

#pack the toolchain for speed up next builds


# move the rootfs.img and tar.gz to workspace
mv ~/buildroot/output/images/rootfs.tar.gz  /opt/rg35xx-toolchain/
mv ~/buildroot/output/images/rootfs.ext2  /opt/rg35xx-toolchain/

cd /opt/
tar -czvf rg35xx-toolchain.tar.gz rg35xx-toolchain/
printf "rg35xx-toolchain.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"