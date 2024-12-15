#!/bin/sh

TARGET=$1
cd /opt/
tar -czvf r36s_toolchain_glibc.tar.gz r36s-toolchain/

rm -rf /opt/r36s-toolchain/rootfs.ext2
rm -rf /opt/r36s-toolchain/rootfs.tar.gz


printf "r36s_toolchain.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"