#!/bin/sh

cd /opt/
tar -czvf my282_toolchain_glibc.tar.gz my282-toolchain/

rm -rf /opt/my282-toolchain/rootfs.ext2
rm -rf /opt/my282-toolchain/rootfs.tar.gz


printf "my282_toolchain.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"