#!/bin/sh

cd ~

BUILDROOT_VERSION=buildroot-2019.11.3
wget https://buildroot.org/downloads/$BUILDROOT_VERSION.tar.gz
tar -xf ./$BUILDROOT_VERSION.tar.gz
rm -f ./$BUILDROOT_VERSION.tar.gz
mv ./$BUILDROOT_VERSION ./buildroot
cd ./buildroot
patch -p1 < ../libpng12.patch
cp ~/miyoomini.config ./.config
