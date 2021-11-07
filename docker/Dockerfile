FROM debian:bullseye

VOLUME /src

WORKDIR /src

ADD ./i3status_build /usr/local/bin

RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        autoconf \
        libconfuse-dev \
        libyajl-dev \
        libasound2-dev \
        libiw-dev \
        asciidoc \
        libpulse-dev \
        libnl-genl-3-dev \
        meson \
        gcc && \
    rm -rf /var/lib/apt/lists/*

ENTRYPOINT /usr/local/bin/i3status_build
