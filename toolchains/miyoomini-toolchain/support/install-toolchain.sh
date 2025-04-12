#!/bin/sh

mkdir -p /opt/miyoomini-toolchain
if [ -d /opt/miyoomini-toolchain/usr ]; then
	rm -fr /opt/miyoomini-toolchain/usr
fi
cp -rf ~/buildroot/output/host/* /opt/miyoomini-toolchain/
# this version of buildroot doesn't have relocate-sdk.sh yet so we bring our own
cd /opt/miyoomini-toolchain
ln -s ./ usr

if [ -f ~/hwcap.h ]; then
	cp ~/hwcap.h arm-buildroot-linux-gnueabihf/sysroot/usr/include/asm/
fi

if [ -f ~/my283.tar.gz ]; then
    cd ~
    tar -xf ~/my283.tar.gz
    cp -n ~/my283/usr/include/* /opt/miyoomini-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/include/
    cp -n ~/my283/usr/lib/*  /opt/miyoomini-toolchain/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/
    cd /opt/miyoomini-toolchain
    
fi


cp ~/relocate-sdk.sh /opt/miyoomini-toolchain/
cp ~/sdk-location /opt/miyoomini-toolchain/
/opt/miyoomini-toolchain/relocate-sdk.sh

#pack the toolchain for speed up next builds


# move the rootfs.img and tar.gz to workspace
mv ~/buildroot/output/images/rootfs.tar.gz  /opt/miyoomini-toolchain/
mv ~/buildroot/output/images/rootfs.ext2  /opt/miyoomini-toolchain/

cd /opt/
tar -czvf miyoomini_toolchain_glibc.tar.gz miyoomini-toolchain/
printf "m21_toolchain_glibc.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"




