# Miyoo Mini Toolchain Buildroot Docker Image

This repo downloads and configures buildroot to build libraries for the [Miyoo Mini toolchain](https://github.com/shauninman/union-miyoomini-toolchain) and optionally packages them up into a full toolchain.tar.xz (slow to compress but half the size of tar.gz).

## Installation

With Docker installed and running, `make shell` builds the toolchain and drops into a shell inside the container. The container's `~/workspace` is bound to `./workspace` by default. Buildroot is downloaded to `~/buildroot`.

After building the first time, `make shell` will skip building and drop into the shell.

## Workflow

`cd` into `~/buildroot` and run `make sdk` (or optionally `make menuconfig` first to add additional libraries).

Then you can copy out individual binaries, libraries, and headers from `~/buildroot/output/host/arm-buildroot-linux-gnueabihf/sysroot/`
or return to `~` and run `./package-toolchain.sh` to generate `~/workspace/miyoomini-toolchain.tar.xz`.

eg.

	make shell
	cd ~/buildroot
	make menuconfig
	make sdk
	cd ~
	./package-toolchain.sh
	exit
