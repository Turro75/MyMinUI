FROM debian:bookworm-slim
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
	git \
	libncurses5-dev \
	automake \
	make \
	rsync \
	scons \
	tree \
	unzip \
	wget \
	zip \
	libjson-c-dev \
	mc \
  && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /root/workspace
WORKDIR /root

COPY support .
#RUN ./build-toochain.sh
RUN ./setup-toolchain.sh
RUN cat setup-env.sh >> .bashrc

VOLUME /root/workspace
WORKDIR /root/workspace

CMD ["/bin/bash"]