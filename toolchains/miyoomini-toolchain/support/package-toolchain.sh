#!/bin/sh

cd /opt/
tar -czvf miyoomini_toolchain_glibc.tar.gz miyoomini-toolchain/

rm -rf /opt/miyoomini-toolchain/rootfs.ext2
rm -rf /opt/miyoomini-toolchain/rootfs.tar.gz


printf "miyoomini_toolchain.tar.gz can be shared as a blob\nby placing in support before calling 'make shell'\n"