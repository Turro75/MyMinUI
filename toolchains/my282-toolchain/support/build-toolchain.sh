#!/bin/sh

BUILDROOT_VERSION=2016.11

set -xe

if [ -d ~/buildroot ]; then
	rm -rf ~/buildroot
else
	sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen
	locale-gen
fi

cd ~

BUILDROOT_NAME=buildroot-$BUILDROOT_VERSION
wget https://buildroot.org/downloads/$BUILDROOT_NAME.tar.gz
tar -xf ./$BUILDROOT_NAME.tar.gz
rm -f ./$BUILDROOT_NAME.tar.gz
mv ./$BUILDROOT_NAME ./buildroot

mkdir -p ~/buildroot/package/gcc/7.2.0
mkdir -p ~/buildroot/package/glibc/2.24/
# patches for buildroot packages
cd ~/patches

for FILE in $(find . -type f -name "*.patch" 2>/dev/null); do
	cp $FILE ~/buildroot/$FILE
done

cd ~/buildroot
# patches for buildroot itself
patch -p1 < ~/sdl2-update-to-2.30.11.patch
patch -p1 < ~/patchelf-update-to-0.9.patch
patch -p1 < ~/gcc-update-to-7.2.0.patch
#patch -p1 < ~/toolchain-expose-BR2_TOOLCHAIN_EXTRA_EXTERNAL_LIBS-for-all-toolchain-types-2017.11.1.diff
#patch -p1 < ~/001-evtest-slow-site.patch

cp ~/my282-buildroot-glibc-$BUILDROOT_VERSION.config ./.config

export FORCE_UNSAFE_CONFIGURE=1
make oldconfig
#make -j8 world
#~/install-toolchain.sh

