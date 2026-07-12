#!/bin/sh

TARGET=$1
cd /opt/
tar -czvf h700_toolchain_glibc.tar.gz h700-toolchain/

rm -rf /opt/h700-toolchain/rootfs.ext2
rm -rf /opt/h700-toolchain/rootfs.tar.gz


printf "h700_toolchain.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"