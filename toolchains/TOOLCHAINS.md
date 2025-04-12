# short info about the provided toolchains.

all toolchains are base on minui toolchain creation.
I modified a bit the build process to make them easier to adapt to other scenarios.

In the docker files there are 2 lines You can edit to build or reuse a prebuild image.

>RUN ./build-toolchain.sh\
#RUN ./setup-toolchain.sh

The above makes the whole docker image from scratch, the process is quite slow it takes roughly 20mins on mac mini m2.
That would be enough but it is not friendly on space size as every image is taking roughly 6GB of space.
to shrink down the image size You need to manually handling every toolchain.

# Manual building of the toolchain:

Open a terminal then go to the toolchain directory i.e. for the rg35xx use the dir toolchains/rg35xx-toolchain

edit the dockerfile to enable the build-toolchain:
> RUN ./build-toolchain.sh \
#RUN ./setup-toolchain.sh

then start to run some commands:

> make clean 

this command deletes the docker image, skip this step if You already built the toolchain

> make shell

this command will run/create the docker image

once finished You are in the image running
> cp /opt/rg35xx_tool* /root/workspace

this command copy the relevant part of the toolchain to the workspace directory

ensure the file has been copied then
> exit

to exit from the docker image

move the file /toolchains/rg35xx-toolchain/workspace/rg35xx_toolchain_glibc.tar.gz to the folder toolchains/rg35xx-toolchain/support

> make clean

this command deletes the docker image

>docker system prune

This command clean the docker cached data saving a lot of disk space.


Edit the dockerfile to enable the setup-toolchain
>#RUN ./build-toolchain.sh \
RUN ./setup-toolchain.sh

then a 

> make shell

will quickly create a new docker image by using the tar.gz just created above.


# Apple silicon special case 
If You are working on a silicon mac You can skip the toolchain:

Edit the dockerfile to enable the setup-toolchain
>#RUN ./build-toolchain.sh \
RUN ./setup-toolchain.sh

then running 
>make shell

it will download the pre made images available here: https://github.com/Turro75/MyMinUI_Toolchains/releases


# SJgam special case

MyMinUI runs on these devices by chrooting a root filesystem based on glibc instead of running on the native platform that is based on the musl libc.

the chroot is set by the emulationstation (m21) or tomato (m22pro) scripts in the sdcard.

Due to that the sjgam m21/m22pro provides both toolchains musl and glibc

Use musl to build binaries that can natively run on the stock system

Use glibc to build binaries that can run on MyMinUI. 


The defalt setting of the dockerfile is

>RUN ./setup-toolchain.sh GLIBC\
#RUN ./setup-toolchain.sh MUSL\
#RUN ./build-toolchain.sh GLIBC\
#RUN ./build-toolchain.sh MUSL

so You can build and setup both systems.

Be aware that You cannot keep both toolchains active at the same.

After switching ensure all m21/cores/output and m21/cores/src folders are empty to rebuild all the cores from scratch.

