FROM debian:buster-slim
ENV DEBIAN_FRONTEND=noninteractive

ENV TZ=Europe/Rome
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get -y update && apt-get -y install \
	bc \
    build-essential \
    bzip2 \
	bzr \
	cmake \
	cmake-curses-gui \
	cpio \
	device-tree-compiler \
	git \
	imagemagick \
	libncurses5-dev \
	locales \
	make \
	p7zip-full \
	rsync \
	sharutils \
	scons \
	tree \
	unzip \
	vim \
	wget \
	mc \
	zip \
  && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /root/workspace
WORKDIR /root

COPY support .
VOLUME /root/workspace
RUN ./setup-toolchain.sh 
#RUN ./build-toolchain.sh 

RUN cat ./setup-env.sh >> .bashrc

WORKDIR /root/workspace

CMD ["/bin/bash"]